#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "dynbitset.h"
#include "translate.h"
#include "dataflow.h"

using namespace std;

vector<unique_ptr<BB>> partition(const vector<Quad> &quads)
{
	map<string, BB*> table;
	vector<unique_ptr<BB>> blocks;
	int n = 0;
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
		BB bb(n++);
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
		if (!lastq.isjump() && next(it) != blocks.end()) {
			BB *f = next(it)->get();
			bb->succ.push_back(f);
			f->pred.push_back(bb.get());
		}
		if (lastq.is_jump_or_branch()) {
			BB *t = table[static_cast<LabelOperand*>(lastq.c)->label];
			bb->succ.push_back(t);
			t->pred.push_back(bb.get());
		}
	}

	for (int i=0; i<n; i++) {
		vector<Quad> &quads = blocks[i]->quads;
		// remove labels
		auto it_label = quads.begin();
		while (it_label != quads.end() && it_label->op == Quad::LABEL)
			it_label++;
		quads.erase(quads.begin(), it_label);
		// remove final unconditional jump if present
		if (!quads.empty() && quads.back().isjump())
			quads.pop_back();
	}
	return blocks;
}

void split_edges(vector<unique_ptr<BB>> &blocks)
{
	int id = blocks.size();
	vector<unique_ptr<BB>> newblocks;
	for (const unique_ptr<BB> &p: blocks) {
		BB *bb = p.get();
		if (bb->succ.size() > 1) {
			for (BB *&succ: bb->succ) {
				if (succ->pred.size() > 1) {
					auto it_pred = find(succ->pred.begin(), succ->pred.end(), bb);
					assert(it_pred != succ->pred.end());
					BB *newbb = new BB(id++);
					newbb->succ.push_back(succ);
					newbb->pred.push_back(bb);
					succ = newbb;
					*it_pred = newbb;
					newblocks.emplace_back(newbb);
				}
			}
		}
	}
	move(newblocks.begin(), newblocks.end(), inserter(blocks, blocks.end()));
}

void compute_def(const Quad &q, dynbitset &ret)
{
	auto def = [&](Operand *o) {
		if (o->istemp()) {
			int id = astemp(o)->id;
			ret.set(8+id);
		}
	};
	switch (q.op) {
	case Quad::DIV:
		def(eax);
		def(edx);
		break;
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MULW:
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
	case Quad::SYNCM:
	case Quad::SYNCR:
		break;
	case Quad::CALL:
		def(eax);
		def(ecx);
		def(edx);
		break;
	case Quad::CDQ:
		def(edx);
		break;
	case Quad::MULB:
		def(eax);
		break;
	default:
		assert(0);
	}
}

int compute_def_temp(const Quad &q)
{
	Operand *o;
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MULW:
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
		o = q.c;
		if (o->istemp()) {
			int id = astemp(o)->id;
			if (id >= 0)
				return id;
		}
		/* fallthrough */
	case Quad::DIV:
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
	case Quad::JMP:
	case Quad::CALL:
	case Quad::PUSH:
	case Quad::CDQ:
	case Quad::MULB:
	case Quad::LABEL:
	case Quad::SYNCM:
	case Quad::SYNCR:
		return -1;
	default:
		assert(0);
	}
}

void use_operand(Operand *o, function<void(int)> f);
void usemem(MemOperand *m, function<void(int)> f)
{
	if (m->base)
		use_operand(m->base, f);
	if (m->index)
		use_operand(m->index, f);
}
void use_operand(Operand *o, function<void(int)> f)
{
	if (o->istemp()) {
		f(astemp(o)->id);
	} else if (o->ismem()) {
		usemem(static_cast<MemOperand*>(o), f);
	}
}

void for_each_use(const Quad &q, function<void(int)> f)
{
	switch (q.op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MULW:
		use_operand(q.c, f);
		use_operand(q.a, f);
		break;
	case Quad::NEG2:
	case Quad::MOV:
	case Quad::LEA:
	case Quad::SEX:
		if (q.c->ismem())
			usemem(static_cast<MemOperand*>(q.c), f);
		use_operand(q.a, f);
		break;
	case Quad::DIV:
		use_operand(eax, f);
		use_operand(edx, f);
		use_operand(q.c, f);
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
		use_operand(q.a, f);
		use_operand(q.b, f);
		break;
	case Quad::JMP:
	case Quad::CALL:
	case Quad::LABEL:
	case Quad::SYNCM:
	case Quad::SYNCR:
		break;
	case Quad::NEG:
	case Quad::PUSH:
	case Quad::INC:
	case Quad::DEC:
		use_operand(q.c, f);
		break;
	case Quad::CDQ:
		use_operand(eax, f);
		break;
	case Quad::MULB:
		use_operand(eax, f);
		use_operand(q.c, f);
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

Graph TranslateEnv::build_interference_graph()
{
	size_t n = quads.size();
	map<string, int> labelmap;
	vector<dynbitset> def(n, dynbitset(8+tempid));
	vector<dynbitset> use(n, dynbitset(8+tempid));
	vector<dynbitset> out(n, dynbitset(8+tempid));
	vector<dynbitset> in (n, dynbitset(8+tempid));
	vector<vector<int>> succ(n);
	for (size_t i=0; i<n; i++) {
		const Quad &q = quads[i];
		if (q.op == Quad::LABEL) {
			labelmap[static_cast<LabelOperand*>(q.c)->label] = i;
		} else {
			compute_def(q, def[i]);
			for_each_use(q, [&](int id){use[i].set(8+id);});
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
	Graph ig(tempid);
	for (size_t i=0; i<n; i++) {
		def[i].foreach([&](int tdef) {
			out[i].foreach([&](int tlive) {
				if (tdef != tlive) {
					ig.connect(tdef-8, tlive-8);
				}
			});
		});
	}
	for (int id: part_temps) {
#ifdef DEBUG
		fprintf(stderr, "part temp: %d\n", id);
#endif
		ig.connect(id, ~4);
		ig.connect(id, ~5);
		ig.connect(id, ~6);
		ig.connect(id, ~7);
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
		/* fallthrough */
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
	case Quad::SYNCM:
	case Quad::SYNCR:
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

void dump_cfg(const string &procname, const vector<unique_ptr<BB>> &blocks)
{
	char fname[80];
	sprintf(fname, "cfg-%s.dot", procname.c_str());
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}
	fprintf(fp, "digraph {\n");
	// declare all basic blocks
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		const BB *bb = it->get();
		stringstream ss;
		for (const Quad &q: bb->quads) {
			ss << q.tostr() << "\\n";
		}
		fprintf(fp, "\tB%d [shape=box label=\"Block %d:\\n%s\"];\n", bb->id, bb->id, ss.str().c_str());
	}
	// arcs
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		const BB *bb = it->get();
		int nsucc = bb->succ.size();
		const char *noyes[2] = { "no", "yes" };
		for (int i=0; i<nsucc; i++) {
			BB *succ = bb->succ[i];
			const vector<BB*> &pred(succ->pred);
			int j = find(pred.begin(), pred.end(), bb)-pred.begin();
			fprintf(fp, "\tB%d -> B%d [label=\"%s(%d)\"];\n", bb->id, succ->id, nsucc == 2 ? noyes[i] : "", j);
		}
	}
	fprintf(fp, "}\n");
	fclose(fp);
}
