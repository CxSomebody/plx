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
		if (o->kind == Operand::TEMP && astemp(o)->id >= 0)
			ret.set(astemp(o)->id);
	};
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::DIV:
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
		break;
	case Quad::CALL:
		// TODO defines eax, ecx, edx
		break;
	default:
		assert(0);
	}
}

void compute_use(const Quad &q, dynbitset &ret)
{
	auto use = [&](Operand *o) {
		if (o->kind == Operand::TEMP && astemp(o)->id >= 0) {
			ret.set(astemp(o)->id);
		} else if (o->kind == Operand::MEM) {
			MemOperand *mo = static_cast<MemOperand*>(o);
			if (mo->baset && mo->baset->id >= 0)
				ret.set(mo->baset->id);
			if (mo->index && mo->baset->id >= 0)
				ret.set(mo->index->id);
		}
	};
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
		break;
	case Quad::CALL:
		assert(q.a->kind == Operand::LIST);
		for (Operand *arg: static_cast<ListOperand*>(q.a)->list)
			use(arg);
		break;
	default:
		assert(0);
	}
}

// returns interference graph
vector<vector<int>> local_livevar(const BB &bb, size_t ntemp)
{
	size_t n = bb.quads.size();
	if (n) {
		vector<dynbitset> def(n, dynbitset(ntemp));
		vector<dynbitset> use(n, dynbitset(ntemp));
		vector<dynbitset> out(n, dynbitset(ntemp));
		for (size_t i=0; i<n; i++) {
			compute_def(bb.quads[i], def[i]);
			compute_use(bb.quads[i], use[i]);
		}
		for (size_t i=n-1; i; i--)
			out[i-1] = use[i] | (out[i]-def[i]);
		// print result
		for (size_t i=0; i<n; i++) {
			bb.quads[i].print();
			printf(" def=");
			def[i].print();
			printf(" use=");
			use[i].print();
			printf(" live_out=");
			out[i].print();
			putchar('\n');
		}
	}
	vector<vector<int>> ig;
	return ig;
}
