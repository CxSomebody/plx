#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

struct Operand {
	enum Kind {
		//SYM,
		IMM,
		TEMP,
		ADDR,
		LIST, // for function calls
	} kind;
	virtual void print() = 0;
protected:
	Operand(Kind kind): kind(kind) {}
};

#if 0
struct SymOperand: Operand
{
	Symbol *sym;
	SymOperand(Symbol *sym): Operand(SYM), sym(sym) {}
	void print() override
	{
		sym->print();
	}
};
#endif

struct ImmOperand: Operand
{
	int val;
	ImmOperand(int val): Operand(IMM), val(val) {}
	void print() override
	{
		printf("%d", val);
	}
};

struct TempOperand: Operand
{
	int id;
	TempOperand(int id): Operand(TEMP), id(id) {}
	void print() override
	{
		if (id >= 0) printf("$%d", id);
		else printf("%%%d", ~id); // physical register
	}
};

struct AddrOperand: Operand
{
	TempOperand *base;
	int offset;
	AddrOperand(TempOperand *base, int offset):
		Operand(ADDR), base(base), offset(offset) {}
	void print() override
	{
		base->print();
		printf("%+d", offset);
	}
};

struct ListOperand: Operand
{
	vector<Operand*> list;
	ListOperand(): Operand(LIST) {}
	void print() override
	{
		bool sep = false;
		for (Operand *o: list) {
			if (sep)
				printf(", ");
			o->print();
			sep = true;
		}
	}
};

void Quad::print()
{
	static char opchar[] = {
		[Quad::ADD] = '+',
		[Quad::SUB] = '-',
		[Quad::MUL] = '*',
		[Quad::DIV] = '/',
	};
	switch (op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::DIV:
		c->print();
		printf(" = ");
		a->print();
		printf(" %c ", opchar[op]);
		b->print();
		break;
	case Quad::INDEX:
		c->print();
		printf(" = ");
		a->print();
		putchar('[');
		b->print();
		putchar(']');
		break;
	case Quad::NEG:
		c->print();
		printf(" = ");
		putchar('-');
		a->print();
		break;
	case Quad::MOV:
		c->print();
		printf(" = ");
		a->print();
		break;
	case Quad::MOV_INDEX:
		a->print();
		putchar('[');
		b->print();
		putchar(']');
		printf(" = ");
		c->print();
		break;
	default:
		assert(0);
	}
}

Operand *TranslateEnv::newtemp()
{
	return new TempOperand(tempid++);
}

Operand *SymExpr::translate(TranslateEnv &env)
{
	//return new SymOperand(sym);
	return nullptr;
}

Operand *LitExpr::translate(TranslateEnv &env)
{
	return new ImmOperand(lit);
}

Operand *BinaryExpr::translate(TranslateEnv &env)
{
	Quad::Op qop;
	switch (op) {
	case ADD: qop = Quad::ADD; break;
	case SUB: qop = Quad::SUB; break;
	case MUL: qop = Quad::MUL; break;
	case DIV: qop = Quad::DIV; break;
	case INDEX: qop = Quad::INDEX; break;
	default: assert(0);
	}
	Operand *c = env.newtemp();
	env.quads.emplace_back(qop, left->translate(env), right->translate(env), c);
	return c;
}

Operand *UnaryExpr::translate(TranslateEnv &env)
{
	Quad::Op qop;
	switch (op) {
	case NEG: qop = Quad::NEG; break;
	default: assert(0);
	}
	Operand *c = env.newtemp();
	env.quads.emplace_back(qop, sub->translate(env), nullptr, c);
	return c;
}

Operand *ApplyExpr::translate(TranslateEnv &env)
{
	// TODO
	return nullptr;
}

void EmptyStmt::translate(TranslateEnv &env)
{
}

void CompStmt::translate(TranslateEnv &env)
{
}

void AssignStmt::translate(TranslateEnv &env)
{
}

void CallStmt::translate(TranslateEnv &env)
{
}

void IfStmt::translate(TranslateEnv &env)
{
}

void DoWhileStmt::translate(TranslateEnv &env)
{
}

void ForStmt::translate(TranslateEnv &env)
{
}

void ReadStmt::translate(TranslateEnv &env)
{
}

void WriteStmt::translate(TranslateEnv &env)
{
}

void allocaddr(Block *blk)
{
	int offset = 0;
	for (VarSymbol *vs: blk->vars) {
		int size = vs->type->size();
		int align = vs->type->align();
		offset = (offset-size) & ~(align-1);
		vs->offset = offset;
	}
}

void translate(unique_ptr<Block> &&blk)
{
	allocaddr(blk.get());
	blk->print(0);
}
