#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

struct QuadOperand {
	enum Kind {
		SYM,
		IMM,
		TEMP,
	} kind;
	virtual void print() = 0;
protected:
	QuadOperand(Kind kind): kind(kind) {}
};

struct SymQuadOperand: QuadOperand
{
	Symbol *sym;
	SymQuadOperand(Symbol *sym): QuadOperand(SYM), sym(sym) {}
	void print() override
	{
		sym->print();
	}
};

struct ImmQuadOperand: QuadOperand
{
	int val;
	ImmQuadOperand(int val): QuadOperand(IMM), val(val) {}
	void print() override
	{
		printf("%d", val);
	}
};

struct TempQuadOperand: QuadOperand
{
	int id;
	TempQuadOperand(int id): QuadOperand(TEMP), id(id) {}
	void print() override
	{
		printf("$%d", id);
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

QuadOperand *TranslateEnv::newtemp()
{
	return new TempQuadOperand(tempid++);
}

QuadOperand *SymExpr::translate(TranslateEnv &env)
{
	return new SymQuadOperand(sym);
}

QuadOperand *LitExpr::translate(TranslateEnv &env)
{
	return new ImmQuadOperand(lit);
}

QuadOperand *BinaryExpr::translate(TranslateEnv &env)
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
	QuadOperand *c = env.newtemp();
	env.quads.emplace_back(qop, left->translate(env), right->translate(env), c);
	return c;
}

QuadOperand *UnaryExpr::translate(TranslateEnv &env)
{
	Quad::Op qop;
	switch (op) {
	case NEG: qop = Quad::NEG; break;
	default: assert(0);
	}
	QuadOperand *c = env.newtemp();
	env.quads.emplace_back(qop, sub->translate(env), nullptr, c);
	return c;
}

QuadOperand *ApplyExpr::translate(TranslateEnv &env)
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

void translate(unique_ptr<Block> &&blk)
{
	blk->print(0);
}
