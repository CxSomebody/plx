#include <cassert>
#include <cctype>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "semant.h"
#include "dynbitset.h"
#include "translate.h"
#include "dataflow.h"

#define astemp static_cast<TempOperand*>

using namespace std;

static const char *opins[] = {
	"add",
	"sub",
	"imul",
	"idiv",
	"je",
	"jne",
	"jl",
	"jge",
	"jg",
	"jle",
	"neg",
	"mov",
	"jmp",
	"call",
	"lea",
	"push",
	"inc",
	"dec",
	"movsx",
	"cdq",
};

void TranslateEnv::emit(const char *ins, Operand *dst, Operand *src)
{
	fprintf(outfp, "\t%s\t%s, %s\n", ins,
		dst->tostr().c_str(),
		src->tostr().c_str());
}

void TranslateEnv::emit(const char *ins, Operand *dst)
{
	fprintf(outfp, "\t%s\t%s\n", ins,
		dst->tostr().c_str());
}

void TranslateEnv::emit(const char *ins)
{
	fprintf(outfp, "\t%s\n", ins);
}

#if 1
bool same_reg(Operand *a, Operand *b)
{
#if 0
	return (a->istemp() &&
		b->istemp() &&
		a->size == b->size &&
		astemp(a)->id == astemp(b)->id);
#else
	return a == b;
#endif
}
#endif

Operand *TranslateEnv::resolve(Operand *o)
{
	if (!o)
		return nullptr;
	if (o->istemp()) {
		TempOperand *t = astemp(o);
		if (t->id >= 0) {
			int color = temp_reg[t->id];
			if (color < 0) {
				// spilled
				return new MemOperand(t->size, ebp, temp_offset[t->id]);
			}
			return getphysreg(t->size, color);
		}
		return o;
	}
	if (o->ismem()) {
		MemOperand *m = static_cast<MemOperand*>(o);
		m->base = resolve(m->base);
		m->index = resolve(m->index);
	}
	return o;
}

void TranslateEnv::gencode()
{
	fprintf(stderr, "gencode: %s\n", procname.c_str());
	temp_offset.resize(tempid);
	int offset = -framesize;
	int maxphysreg = -1;
	bool spill;
	int iter = 0;
	do {
		spill = false;
		fprintf(stderr, "iter %d:\n", iter);
		dump_quads();
		rewrite();
		temp_reg = color_graph(global_livevar(quads, tempid));
		// must update maxphysreg after each iteration
		for (int i=0; i<tempid; i++) {
			if (maxphysreg < temp_reg[i])
				maxphysreg = temp_reg[i];
		}
		// allocate address for spilled temporaries
		for (int i=0; i<tempid; i++) {
			if (temp_reg[i] < 0) {
				spill = true;
				int scalar = temp_scalar[i];
				fprintf(stderr, "spill: %d\n", i);
				if (scalar >= 0) {
					fprintf(stderr, "scalar\n");
					temp_offset[i] = scalar_var[scalar]->offset;
				} else {
					int size = temps[i]->size;
					int align = size;
					offset = (offset-size) & ~(align-1);
					temp_offset[i] = offset;
				}
			}
		}
		for (Quad &q: quads) {
			q.c = resolve(q.c);
			q.a = resolve(q.a);
			q.b = resolve(q.b);
		}
		iter++;
	} while (spill);
	fprintf(stderr, "final:\n");
	dump_quads();
	offset &= ~3;
	framesize = -offset;
	fprintf(outfp, "$%s:\n", procname.c_str());
	// prologue
	emit("push", ebp);
	emit("mov", ebp, esp);
	if (framesize) {
		ImmOperand fs(framesize);
		emit("sub", esp, &fs);
	}
	if (maxphysreg >= 3) {
		emit("push", ebx);
		if (maxphysreg >= 6) {
			emit("push", esi);
			if (maxphysreg >= 7) {
				emit("push", edi);
			}
		}
	}
	// body
	for (const Quad &q: quads) {
		switch (q.op) {
		case Quad::MOV:
			if (!same_reg(q.c, q.a))
		case Quad::ADD:
		case Quad::SUB:
		case Quad::MUL:
		case Quad::LEA:
		case Quad::SEX:
				emit(opins[q.op], q.c, q.a);
			break;
		case Quad::DIV:
		case Quad::NEG:
		case Quad::JMP:
		case Quad::CALL:
		case Quad::PUSH:
		case Quad::INC:
		case Quad::DEC:
			emit(opins[q.op], q.c);
			break;
		case Quad::BEQ:
		case Quad::BNE:
		case Quad::BLT:
		case Quad::BGE:
		case Quad::BGT:
		case Quad::BLE:
			emit("cmp", q.a, q.b);
			emit(opins[q.op], q.c);
			break;
		case Quad::CDQ:
			emit(opins[q.op]);
			break;
		case Quad::LABEL:
			fprintf(outfp, "%s:\n", static_cast<LabelOperand*>(q.c)->label.c_str());
			break;
		default:
			assert(0);
		}
	}
	// epilogue
	if (maxphysreg >= 7)
		emit("pop", edi);
	if (maxphysreg >= 6)
		emit("pop", esi);
	if (maxphysreg >= 3)
		emit("pop", ebx);
	emit("leave");
	emit("ret");
}

string ImmOperand::tostr() const
{
	char tmp[12];
	sprintf(tmp, "%d", val);
	return string(tmp);
}

const char *regname4[8] = {
	"eax", "ecx", "edx", "ebx",
	"esp", "ebp", "esi", "edi",
};

const char *regname1[4] = {
	"al", "cl", "dl", "bl",
};

string TempOperand::tostr() const
{
	if (id < 0) {
		int phy = ~id;
		if (size == 4) {
			assert(phy < 8);
			return regname4[phy];
		}
		if (size == 1) {
			assert(phy < 4);
			return regname1[phy];
		}
		assert(0);
	} else {
		char tmp[12];
		sprintf(tmp, "%d$%d", size, id);
		return string(tmp);
	}
}

string LabelOperand::tostr() const
{
	return isalpha(label[0]) ? '$'+label : label;
}

string MemOperand::tostr() const
{
	stringstream ss;
	if (size == 4) {
		ss << "dword ";
	} else if (size == 1) {
		ss << "byte ";
	} else if (size == 0) {
		/* do nothing */
	} else {
		assert(0);
	}
	ss << '[';
	bool sep = false;
	if (base) {
		ss << base->tostr();
		sep = true;
	}
	if (offset) {
		char tmp[12];
		sprintf(tmp, sep?"%+d":"%d", offset);
		ss << tmp;
		sep = true;
	}
	if (index) {
		if (sep)
			ss << '+';
		ss << index->tostr();
		ss << '*' << scale;
	}
	ss << ']';
	return ss.str();
}
