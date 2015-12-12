#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "translate.h"
#include "dynbitset.h"
#include "dataflow.h"

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

void compute_def(const Quad &q, dynbitset &ret, bool exclude_physreg)
{
	auto def = [&](Operand *o) {
		if (o->istemp()) {
			int id = astemp(o)->id;
			if (exclude_physreg) {
				if (id >= 0)
					ret.set(id);
			} else {
				ret.set(8+id);
			}
		}
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
	case Quad::ADD3:
	case Quad::SUB3:
	case Quad::MUL3:
	case Quad::DIV3:
	case Quad::NEG2:
	case Quad::PHI:
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
	case Quad::LABEL:
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

void compute_use(const Quad &q, dynbitset &ret, bool exclude_physreg)
{
	auto usetemp = [&](TempOperand *t) {
		int id = t->id;
		if (exclude_physreg) {
			if (id >= 0)
				ret.set(id);
		} else {
			ret.set(8+id);
		}
	};
	auto usemem = [&](MemOperand *m) {
		if (m->base) {
			if (!m->base->islabel()) {
				assert(m->base->istemp());
				usetemp(astemp(m->base));
			}
		}
		if (m->index) {
			assert(m->index->istemp());
			usetemp(astemp(m->index));
		}
	};
	auto use = [&](Operand *o) {
		if (o->istemp()) {
			usetemp(astemp(o));
		} else if (o->ismem()) {
			usemem(static_cast<MemOperand*>(o));
		}
	};
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::MOV:
	case Quad::NEG2:
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
	case Quad::ADD3:
	case Quad::SUB3:
	case Quad::MUL3:
	case Quad::DIV3:
		use(q.a);
		use(q.b);
		break;
	case Quad::JMP:
	case Quad::CALL:
	case Quad::LABEL:
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

void replace_def(Quad &q, int old, int neu)
{
	auto replace = [=](Operand *&o) {
		if (o->istemp() && astemp(o)->id == old)
			o = new TempOperand(o->size, neu);
	};
	switch (q.op) {
	case Quad::ADD3:
	case Quad::SUB3:
	case Quad::MUL3:
	case Quad::DIV3:
	case Quad::NEG2:
	case Quad::MOV:
	case Quad::LEA:
	case Quad::INC:
	case Quad::DEC:
	case Quad::SEX:
	case Quad::PHI:
		replace(q.c);
		break;
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
	case Quad::JMP:
	case Quad::PUSH:
	case Quad::LABEL:
	case Quad::CALL:
		break;
	default:
		assert(0);
	}
}

void replace_temp(Operand *&o, int old, int neu)
{
	assert(o->istemp());
	if (astemp(o)->id == old)
		o = new TempOperand(o->size, neu);
}

void replace(Operand *&o, int old, int neu);

void replace_mem(MemOperand *m, int old, int neu)
{
	if (m->base)
		replace(m->base, old, neu);
	if (m->index)
		replace(m->index, old, neu);
}

void replace(Operand *&o, int old, int neu)
{
	if (o->istemp()) {
		replace_temp(o, old, neu);
	} else if (o->ismem()) {
		replace_mem(static_cast<MemOperand*>(o), old, neu);
	}
}

void replace_use(Quad &q, int old, int neu)
{
	switch (q.op) {
	case Quad::NEG2:
	case Quad::MOV:
	case Quad::LEA:
	case Quad::SEX:
		if (q.c->ismem())
			replace_mem(static_cast<MemOperand*>(q.c), old, neu);
		replace(q.a, old, neu);
		break;
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
		replace(q.a, old, neu);
		replace(q.b, old, neu);
		break;
	case Quad::ADD3:
	case Quad::SUB3:
	case Quad::MUL3:
	case Quad::DIV3:
		if (q.c->ismem())
			replace_mem(static_cast<MemOperand*>(q.c), old, neu);
		replace(q.a, old, neu);
		replace(q.b, old, neu);
		break;
	case Quad::PUSH:
		replace(q.c, old, neu);
		break;
	case Quad::JMP:
	case Quad::CALL:
	case Quad::LABEL:
		break;
	default:
		assert(0);
	}
}
