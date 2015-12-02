#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"
#include "dynbitset.h"

#define astemp static_cast<TempOperand*>

using namespace std;

bool istemp(Operand *o)
{
	return o->kind == Operand::TEMP;
}

vector<unique_ptr<BB>> partition(const vector<Quad> &quads)
{
	map<string, BB*> table;
	vector<unique_ptr<BB>> blocks;
	int id = 0;
	auto begin = quads.begin();
	auto end = begin;
	for (;;) {
		while (end != quads.end() && end->op == Quad::LABEL)
			end++;
		while (end != quads.end() && end->op != Quad::LABEL && !end->is_jump_or_branch())
			end++;
		if (end != quads.end() && end->is_jump_or_branch())
			end++;
		if (end == begin)
			break;
		BB bb;
		bb.id = id++;
		bb.quads.insert(bb.quads.end(), begin, end);
		blocks.push_back(make_unique<BB>(move(bb)));
		begin = end;
	}
	// set up lookup table
	for (const unique_ptr<BB> &bb: blocks) {
		for (auto it = bb->quads.begin();
		     it != bb->quads.end() && it->op == Quad::LABEL;
		     it++)
		{
			const string &label = static_cast<LabelOperand*>(it->c)->label;
			table[label] = bb.get();
		}
	}
	// compute pred & succ
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		const unique_ptr<BB> &bb = *it;
		assert(!bb->quads.empty());
		const Quad &lastq = bb->quads.back();
		if (lastq.is_jump_or_branch()) {
			BB *t = table[static_cast<LabelOperand*>(lastq.c)->label];
			bb->succ.push_back(t);
			t->pred.push_back(bb.get());
		}
		if (!lastq.isjump() && next(it) != blocks.end()) {
			BB *f = next(it)->get();
			bb->succ.push_back(f);
			f->pred.push_back(bb.get());
		}
	}
	return blocks;
}

#if 0
vector<vector<int>> livevar(const vector<unique_ptr<BB>> &blocks, int n)
{
}
#endif

void compute_def(const Quad &q, dynbitset &ret)
{
	auto def = [&](Operand *o) {
		if (o->kind == Operand::TEMP)
			ret.set(8+astemp(o)->id);
	};
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	//case Quad::DIV:
	case Quad::NEG:
	case Quad::MOV:
	case Quad::LEA:
		def(q.c);
		/* fallthrough */
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
	case Quad::JMP:
	case Quad::LABEL:
	case Quad::PUSH:
		break;
	case Quad::CALL:
		ret.set(8+~0); // eax
		ret.set(8+~1); // ecx
		ret.set(8+~2); // edx
		break;
	case Quad::DIV:
		def(q.c);
		ret.set(8+~0); // eax
		ret.set(8+~2); // edx
		break;
	default:
		assert(0);
	}
}

void compute_use(const Quad &q, dynbitset &ret)
{
	auto usemem = [&](MemOperand *m) {
		if (m->baset)
			ret.set(8+m->baset->id);
		if (m->index)
			ret.set(8+m->index->id);
	};
	auto use = [&](Operand *o) {
		if (o->kind == Operand::TEMP) {
			ret.set(8+astemp(o)->id);
		} else if (o->kind == Operand::MEM) {
			usemem(static_cast<MemOperand*>(o));
		}
	};
	if (q.c && q.c->kind == Operand::MEM)
		usemem(static_cast<MemOperand*>(q.c));
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::DIV:
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
		use(q.b);
		/* fallthrough */
	case Quad::NEG:
	case Quad::MOV:
	case Quad::LEA:
		use(q.a);
		/* fallthrough */
	case Quad::JMP:
	case Quad::LABEL:
	case Quad::CALL:
		break;
	case Quad::PUSH:
		use(q.c);
		break;
	default:
		assert(0);
	}
}

void Graph::connect(int a, int b)
{
	assert(a >= -8 && a < end);
	assert(b >= -8 && b < end);
	vector<int> &na = neighbors(a);
	if (find(na.begin(), na.end(), b) == na.end()) {
		na.push_back(b);
		neighbors(b).push_back(a);
	}
}

void printtemp(int t)
{
	if (t<0) {
		printf("%s", regname[~t]);
	} else {
		printf("$%d", t);
	}
};

void Graph::print() const
{
	for (int i=-8; i<end; i++) {
		printtemp(i);
		putchar(':');
		for (int a: neighbors(i)) {
			putchar(' ');
			printtemp(a);
		}
		putchar('\n');
	}
}

vector<int> Graph::remove(int v)
{
	assert(v >= -8 && v < end);
	for (int a: neighbors(v)) {
		vector<int> &na = neighbors(a);
		na.erase(find(na.begin(), na.end(), v));
	}
	vector<int> &nv = neighbors(v);
	vector<int> ret(move(nv));
	nv.clear();
	return ret;
}

void print_bitset(const dynbitset &bs)
{
	putchar('[');
	bool sep = false;
	for (size_t i=0; i<bs.size(); i++) {
		int index = i;
		if (bs.get(index)) {
			if (sep)
				putchar(',');
			printtemp(index-8);
			sep = true;
		}
	}
	putchar(']');
}

// returns interference graph
void local_livevar(const BB &bb, size_t ntemp, Graph &ig)
{
	size_t n = bb.quads.size();
	vector<dynbitset> def(n, dynbitset(8+ntemp));
	vector<dynbitset> use(n, dynbitset(8+ntemp));
	vector<dynbitset> out(n, dynbitset(8+ntemp));
	if (n) {
		for (size_t i=0; i<n; i++) {
			compute_def(bb.quads[i], def[i]);
			compute_use(bb.quads[i], use[i]);
		}
		for (size_t i=n-1; i; i--)
			out[i-1] = use[i] | (out[i]-def[i]);
		// print result
		for (size_t i=0; i<n; i++) {
			bb.quads[i].print();
			printf(" -- def=");
			print_bitset(def[i]);
			printf(" use=");
			print_bitset(use[i]);
			printf(" live_out=");
			print_bitset(out[i]);
			putchar('\n');
		}
	}
	for (size_t i=0; i<n; i++) {
		def[i].foreach([&](int tdef) {
			out[i].foreach([&](int tlive) {
				if (tdef != tlive) {
					ig.connect(tdef-8, tlive-8);
				}
			});
		});
	}
}
