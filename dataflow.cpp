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

void compute_def(const Quad &q, dynbitset &ret)
{
	auto def = [&](Operand *o) {
		if (o->istemp())
			ret.set(8+astemp(o)->id);
	};
	switch (q.op) {
	case Quad::DIV:
		def(eax);
		def(edx);
		break;
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::NEG:
	case Quad::MOV:
	case Quad::LEA:
	case Quad::INC:
	case Quad::DEC:
	case Quad::SEX:
		def(q.c);
		break;
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
	case Quad::JMP:
	case Quad::PUSH:
		break;
	case Quad::CALL:
		def(eax);
		def(ecx);
		def(edx);
		break;
	case Quad::CDQ:
		def(edx);
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
		if (o->istemp()) {
			ret.set(8+astemp(o)->id);
		} else if (o->ismem()) {
			usemem(static_cast<MemOperand*>(o));
		}
	};
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::MOV:
	case Quad::LEA:
	case Quad::SEX:
		if (q.c->ismem())
			usemem(static_cast<MemOperand*>(q.c));
		use(q.a);
		break;
	case Quad::DIV:
		use(eax);
		use(edx);
		use(q.c);
		break;
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
		use(q.a);
		use(q.b);
		break;
	case Quad::JMP:
	case Quad::CALL:
		break;
	case Quad::NEG:
	case Quad::PUSH:
	case Quad::INC:
	case Quad::DEC:
		use(q.c);
		break;
	case Quad::CDQ:
		use(eax);
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
		printf("%s", regname4[~t]);
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

Graph global_livevar(const vector<Quad> &quads, int ntemp)
{
	size_t n = quads.size();
	map<string, int> labelmap;
	vector<dynbitset> def(n, dynbitset(8+ntemp));
	vector<dynbitset> use(n, dynbitset(8+ntemp));
	vector<dynbitset> out(n, dynbitset(8+ntemp));
	vector<dynbitset> in (n, dynbitset(8+ntemp));
	vector<vector<int>> succ(n);
	for (size_t i=0; i<n; i++) {
		const Quad &q = quads[i];
		if (q.op == Quad::LABEL) {
			labelmap[static_cast<LabelOperand*>(q.c)->label] = i;
		} else {
			compute_def(q, def[i]);
			compute_use(q, use[i]);
		}
	}
	for (size_t i=0; i<n; i++) {
		const Quad &q = quads[i];
		if (q.is_jump_or_branch())
			succ[i].push_back(labelmap[static_cast<LabelOperand*>(q.c)->label]);
		if (!q.isjump() && i != n-1)
			succ[i].push_back(i+1);
	}
	bool changed;
	do {
		changed = false;
		for (size_t j=n; j; j--) {
			size_t i = j-1;
			in[i] = use[i] | (out[i]-def[i]);
		}
		for (size_t i=0; i<n; i++) {
			for (int s: succ[i]) {
				if (out[i].add_all(in[s]))
					changed = true;
			}
		}
	} while (changed);
#if 0
	for (size_t i=0; i<n; i++) {
		quads[i].print();
		printf(" -- def=");
		print_bitset(def[i]);
		printf(" use=");
		print_bitset(use[i]);
		printf(" live_out=");
		print_bitset(out[i]);
		putchar('\n');
	}
#endif
	Graph ig(ntemp);
	for (size_t i=0; i<n; i++) {
		def[i].foreach([&](int tdef) {
			out[i].foreach([&](int tlive) {
				if (tdef != tlive) {
					ig.connect(tdef-8, tlive-8);
				}
			});
		});
	}
	return ig;
}
