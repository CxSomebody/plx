#include <cassert>
#include <cctype>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
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
		dst->gencode(*this).c_str(),
		src->gencode(*this).c_str());
}

void TranslateEnv::emit(const char *ins, Operand *dst)
{
	fprintf(outfp, "\t%s\t%s\n", ins,
		dst->gencode(*this).c_str());
}

void TranslateEnv::emit(const char *ins)
{
	fprintf(outfp, "\t%s\n", ins);
}

void TranslateEnv::emit_mov(Operand *dst, Operand *src)
{
	if (!(dst->istemp() &&
	      src->istemp() &&
	      physreg(astemp(dst)) ==
	      physreg(astemp(src))))
		emit("mov", dst, src);
}

void TranslateEnv::gencode()
{
#if 0
	auto bblisttostr = [](const vector<BB*> &list) {
		stringstream ss;
		ss << '[';
		bool sep = false;
		for (BB *bb: list) {
			if (sep)
				ss << ',';
			ss << bb->id;
			sep = true;
		}
		ss << ']';
		return ss.str();
	};
	vector<unique_ptr<BB>> bbs = partition(quads);
	for (const unique_ptr<BB> &bb: bbs) {
		printf("=== BLOCK %d pred=%s succ=%s ===\n", bb->id,
		       bblisttostr(bb->pred).c_str(),
		       bblisttostr(bb->succ).c_str());
		printf("tempid=%d\n", tempid);
	}
#endif
	temp_reg = color_graph(global_livevar(*this, tempid));
	int maxphysreg = -1;
	for (int i=0; i<tempid; i++) {
		if (maxphysreg < temp_reg[i])
			maxphysreg = temp_reg[i];
	}
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
		case Quad::MOV:
			emit_mov(q.c, q.a);
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

string ImmOperand::gencode(TranslateEnv &env) const
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

string TempOperand::gencode(TranslateEnv &env) const
{
	int phy = env.physreg(this);
	assert(phy >= 0);
	if (size == 4) {
		assert(phy < 8);
		return regname4[phy];
	}
	if (size == 1) {
		assert(phy < 4);
		return regname1[phy];
	}
	assert(0);
}

int TranslateEnv::physreg(const TempOperand *t)
{
	return t->id < 0 ? ~t->id : temp_reg[t->id];
}

string LabelOperand::gencode(TranslateEnv &env) const
{
	return isalpha(label[0]) ? '$'+label : label;
}

string MemOperand::gencode(TranslateEnv &env) const
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
	if (baset) {
		ss << baset->gencode(env);
		sep = true;
	}
	if (basel) {
		if (sep)
			ss << '+';
		ss << basel->gencode(env);
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
		ss << index->gencode(env);
		ss << '*' << scale;
	}
	ss << ']';
	return ss.str();
}
