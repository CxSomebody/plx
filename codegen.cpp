#include <cassert>
#include <cctype>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"

using namespace std;

static const char *opins[] = {
	"add",
	"sub",
	"imul",
	nullptr, // idiv
	"je",
	"jne",
	"jl",
	"jge",
	"jg",
	"jle",
	"neg",
	"mov",
	"jmp",
	nullptr, // call
	"lea",
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
	if (!(dst->kind == Operand::TEMP &&
	      src->kind == Operand::TEMP &&
	      physreg(static_cast<TempOperand*>(dst)) ==
	      physreg(static_cast<TempOperand*>(src))))
		emit("mov", dst, src);
}

void TranslateEnv::gencode()
{
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
	Graph ig(tempid);
	for (const unique_ptr<BB> &bb: bbs) {
		printf("=== BLOCK %d pred=%s succ=%s ===\n", bb->id,
		       bblisttostr(bb->pred).c_str(),
		       bblisttostr(bb->succ).c_str());
		printf("tempid=%d\n", tempid);
		local_livevar(*bb, tempid, ig);
	}
	temp_reg = color_graph(move(ig));
	int maxphysreg = -1;
	for (int i=0; i<tempid; i++) {
		if (maxphysreg < temp_reg[i])
			maxphysreg = temp_reg[i];
	}
	fprintf(outfp, "%s:\n", procname.c_str());
	TempOperand *eax = getphysreg(0);
	TempOperand *edx = getphysreg(2);
	TempOperand *ebx = getphysreg(3);
	TempOperand *esp = getphysreg(4);
	TempOperand *ebp = getphysreg(5);
	TempOperand *esi = getphysreg(6);
	TempOperand *edi = getphysreg(7);
	// prologue
	ImmOperand fs(framesize);
	emit("push", ebp);
	emit("mov", ebp, esp);
	emit("sub", esp, &fs);
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
			emit_mov(q.c, q.a);
			emit(opins[q.op], q.c, q.b);
			break;
		case Quad::DIV:
			// dividend in EDX:EAX
			emit_mov(eax, q.a);
			emit("xor", edx, edx);
			emit("idiv", q.b);
			emit_mov(q.c, eax);
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
		case Quad::LEA:
			emit(opins[q.op], q.c, q.a);
			break;
		case Quad::NEG:
		case Quad::JMP:
			emit(opins[q.op], q.c);
			break;
		case Quad::CALL:
			{
				const vector<Operand*> &args = static_cast<ListOperand*>(q.a)->list;
				for (auto it = args.rbegin(); it != args.rend(); it++)
					emit("push", *it);
				emit("call", q.c);
				ImmOperand n(args.size()*4);
				emit("add", esp, &n);
			}
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

const char *regname[8] = {
	"eax", "ecx", "edx", "ebx",
	"esp", "ebp", "esi", "edi",
};

string TempOperand::gencode(TranslateEnv &env) const
{
	int phy = env.physreg(this);
	assert(phy >= 0 && phy < 8);
	return regname[phy];
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
	if (_size == 4) {
		ss << "dword ";
	} else if (_size == 1) {
		ss << "byte ";
	} else if (_size == 0) {
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

string ListOperand::gencode(TranslateEnv &env) const
{
	// invoking gencode on ListOperand does not make sense
	assert(0);
}
