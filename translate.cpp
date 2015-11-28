#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

struct Quad {
	enum Op {
		ADD,
		SUB,
		MUL,
		DIV,
		INDEX,
		NEG,
		MOV,
		MOV_INDEX,
	} op;
	Operand *a, *b, *c; // src1, src2, dst
	Quad(Op op, Operand *a, Operand *b, Operand *c):
		op(op), a(a), b(b), c(c) {}
	void print() const;
};

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

class TranslateEnv {
	SymbolTable *symtab;
	int tempid;
public:
	std::vector<Quad> quads;
	Operand *newtemp();
	TranslateEnv(SymbolTable *symtab): symtab(symtab), tempid(0) {}
	int level() const { return symtab->level; }
};

void Quad::print() const
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

Operand *SymExpr::translate(TranslateEnv &env) const
{
	//return new SymOperand(sym);

	return nullptr;
}

Operand *LitExpr::translate(TranslateEnv &env) const
{
	return new ImmOperand(lit);
}

Operand *BinaryExpr::translate(TranslateEnv &env) const
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

Operand *UnaryExpr::translate(TranslateEnv &env) const
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

Operand *ApplyExpr::translate(TranslateEnv &env) const
{
	// TODO
	return nullptr;
}

void EmptyStmt::translate(TranslateEnv &env) const
{
}

void CompStmt::translate(TranslateEnv &env) const
{
}

void AssignStmt::translate(TranslateEnv &env) const
{
}

void CallStmt::translate(TranslateEnv &env) const
{
}

void IfStmt::translate(TranslateEnv &env) const
{
}

void DoWhileStmt::translate(TranslateEnv &env) const
{
}

void ForStmt::translate(TranslateEnv &env) const
{
}

void ReadStmt::translate(TranslateEnv &env) const
{
}

void WriteStmt::translate(TranslateEnv &env) const
{
}

void Block::allocaddr()
{
	int offset = 0;
	for (VarSymbol *vs: vars) {
		int size = vs->type->size();
		int align = vs->type->align();
		offset = (offset-size) & ~(align-1);
		vs->offset = offset;
	}
}

void Block::translate()
{
	printf("begin %s\n", name.c_str());
	TranslateEnv env(symtab);
	allocaddr();
	for (const unique_ptr<Block> &sub: subs)
		sub->translate();
	for (const unique_ptr<Stmt> &stmt: stmts)
		stmt->translate(env);
	for (const Quad &q: env.quads) {
		q.print();
		putchar('\n');
	}
	printf("end %s\n", name.c_str());
}

void translate_all(unique_ptr<Block> &&blk)
{
	blk->translate();
	blk->print(0);
}
