#include <cassert>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"

using namespace std;

#if 0
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
};
#endif

void TranslateEnv::emit(const string &ins, const string &dst, const string &src)
{
	fprintf(outfp, "\t%s\t%s, %s\n", ins.c_str(), dst.c_str(), src.c_str());
}

void TranslateEnv::emit(const string &ins, const string &dst)
{
	fprintf(outfp, "\t%s\t%s\n", ins.c_str(), dst.c_str());
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
	for (const unique_ptr<BB> &bb: bbs) {
		printf("=== BLOCK %d pred=%s succ=%s ===\n", bb->id,
		       bblisttostr(bb->pred).c_str(),
		       bblisttostr(bb->succ).c_str());
#if 0
		for (const Quad &q: bb->quads) {
			q.print();
			putchar('\n');
		}
#endif
		printf("tempid=%d\n", tempid);
		color_graph(local_livevar(*bb, tempid));
	}
#if 0
	temp_reg.resize(tempid);
	for (const Quad &q: quads) {
		switch (q.op) {
		case Quad::ADD:
		case Quad::SUB:
			{
				string c = q.c->gencode(*this);
				string a = q.a->gencode(*this);
				string b = q.b->gencode(*this);
				emit("mov", c, a);
				emit(opins[q.op], c, b);
			}
			break;
		case Quad::MUL:
		case Quad::DIV:
			TODO("mul/div");
			break;
		case Quad::BEQ:
		case Quad::BNE:
		case Quad::BLT:
		case Quad::BGE:
		case Quad::BGT:
		case Quad::BLE:
			{
				string c = q.c->gencode(*this);
				string a = q.a->gencode(*this);
				string b = q.b->gencode(*this);
				emit("cmp", a, b);
				emit(opins[q.op], c);
			}
			break;
		case Quad::MOV:
			{
				string c = q.c->gencode(*this);
				string a = q.a->gencode(*this);
				emit("mov", c, a);
			}
			break;
		default:
			assert(0);
		}
	}
#endif
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
	int phy = id >= 0 ? env.temp_reg[id] : ~id;
	assert(phy<8);
	return regname[phy];
}

string LabelOperand::gencode(TranslateEnv &env) const
{
	return label;
}

string MemOperand::gencode(TranslateEnv &env) const
{
	stringstream ss;
	if (_size == 4) {
		ss << "dword ";
	} else if (_size == 1) {
		ss << "byte ";
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
