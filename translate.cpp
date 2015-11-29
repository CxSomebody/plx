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
		NEG,
		MOV,
		JMP,
		BEQ,
		BNE,
		BLT,
		BGE,
		BGT,
		BLE,
		CALL,
	} op;
	Operand *c, *a, *b;
	Quad(Op op, Operand *c, Operand *a, Operand *b):
		op(op), c(c), a(a), b(b) {}
	Quad(Op op, Operand *c, Operand *a): Quad(op, c, a, nullptr) {}
	Quad(Op op, Operand *c): Quad(op, c, nullptr, nullptr) {}
	void print() const;
};

struct Operand {
	enum Kind {
		IMM,
		TEMP,
		MEM,
		LABEL,
		LIST, // for function calls
	} kind;
	virtual void print() const = 0;
protected:
	Operand(Kind kind): kind(kind) {}
};

struct ImmOperand: Operand
{
	int val;
	ImmOperand(int val): Operand(IMM), val(val) {}
	void print() const override
	{
		printf("%d", val);
	}
};

struct TempOperand: Operand
{
	int id;
	TempOperand(int id): Operand(TEMP), id(id) {}
	void print() const override
	{
		if (id >= 0) printf("$%d", id);
		else printf("%%%d", ~id); // physical register
	}
};

struct LabelOperand: Operand
{
	string label;
	LabelOperand(const string &label): Operand(LABEL), label(label) {}
	void print() const override
	{
		printf("%s", label.c_str());
	}
};

struct MemOperand: Operand
{
	TempOperand *baset;
	LabelOperand *basel;
	int offset;
	TempOperand *index;
	int scale;
	MemOperand(TempOperand *baset,
		   int offset,
		   TempOperand *index,
		   int scale):
		Operand(MEM),
		baset(baset),
		basel(nullptr),
		offset(offset),
		index(index),
		scale(scale) {}
	MemOperand(TempOperand *baset, int offset):
		MemOperand(baset, offset, nullptr, 0) {}
	MemOperand(LabelOperand *basel,
		   int offset,
		   TempOperand *index,
		   int scale):
		Operand(MEM),
		baset(nullptr),
		basel(basel),
		offset(offset),
		index(index),
		scale(scale) {}
	MemOperand(LabelOperand *basel):
		MemOperand(basel, 0, nullptr, 0) {}
	void print() const override
	{
		putchar('[');
		bool sep = false;
		if (baset) {
			baset->print();
			sep = true;
		}
		if (basel) {
			if (sep)
				putchar('+');
			basel->print();
			sep = true;
		}
		if (offset) {
			printf(sep?"%+d":"%d", offset);
			sep = true;
		}
		if (index) {
			if (sep)
				putchar('+');
			index->print();
			printf("*%d", scale);
		}
		putchar(']');
	}
};

struct ListOperand: Operand
{
	vector<Operand*> list;
	ListOperand(): Operand(LIST) {}
	void print() const override
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

TempOperand *getphysreg(int id)
{
	static TempOperand physreg[8] =
	{{~0}, {~1}, {~2}, {~3}, {~4}, {~5}, {~6}, {~7}};
	return &physreg[id];
}

class TranslateEnv {
	SymbolTable *symtab;
	int tempid;
public:
	std::vector<Quad> quads;
	TempOperand *gettemp();
	TranslateEnv(SymbolTable *symtab): symtab(symtab), tempid(0) {}
	int level() const { return symtab->level; }
};

#define TODO(msg) todo(__FILE__, __LINE__, msg)

static void todo(const char *file, int line, const char *msg)
{
	fprintf(stderr, "%s: %d: TODO: %s\n", file, line, msg);
	abort();
}

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
	case Quad::CALL:
		printf("call ");
		c->print();
		putchar('(');
		a->print();
		putchar(')');
		break;
	default:
		assert(0);
	}
}

TempOperand *TranslateEnv::gettemp()
{
	return new TempOperand(tempid++);
}

Operand *SymExpr::translate(TranslateEnv &env) const
{
	if (sym->kind == Symbol::VAR) {
		VarSymbol *vs = static_cast<VarSymbol*>(sym);
		if (vs->level) {
			// local var
			TempOperand *bp;
			if (vs->level == env.level()) {
				bp = getphysreg(5);
			} else {
				int leveldiff = env.level()-vs->level;
				bp = env.gettemp();
				env.quads.emplace_back(Quad::MOV, bp, new MemOperand(getphysreg(5), 8+4*(leveldiff-1)));
			}
			return new MemOperand(bp, vs->offset);
		}
		// global var
		return new MemOperand(new LabelOperand(vs->name));
	}
	if (sym->kind == Symbol::PROC) {
		// address of function
		// TODO decorate inner function names
		return new LabelOperand(sym->name);
	}
	assert(sym->kind == Symbol::CONST);
	return new ImmOperand(static_cast<ConstSymbol*>(sym)->val);
}

Operand *LitExpr::translate(TranslateEnv &env) const
{
	return new ImmOperand(lit);
}

Operand *BinaryExpr::translate(TranslateEnv &env) const
{
	Operand *c;
	if (op == INDEX) {
		if (left->kind != SYM)
			TODO("non-SYM subscripted value");
		Symbol *arraysym = static_cast<SymExpr*>(left.get())->sym;
		if (arraysym->kind != Symbol::VAR)
			TODO("non-VAR subscripted value");
		Type *arrayty = static_cast<VarSymbol*>(arraysym)->type;
		if (arrayty->kind != Type::ARRAY)
			TODO("non-ARRAY subscripted value");
		int scale = static_cast<ArrayType*>(arrayty)->elemtype->size();
		c = left->translate(env);
		if (c->kind != Operand::MEM) {
			TODO("non-MEM subscripted value");
		}
		MemOperand *moleft = static_cast<MemOperand*>(c);
		assert(!moleft->index);
		Operand *oright = right->translate(env);
		if (oright->kind == Operand::IMM) {
			moleft->offset += static_cast<ImmOperand*>(oright)->val * scale;
		} else if (oright->kind == Operand::TEMP) {
			moleft->index = static_cast<TempOperand*>(oright);
			moleft->scale = scale;
		} else {
			TODO("non-IMM-or-TEMP subscript");
		}
	} else {
		Quad::Op qop;
		switch (op) {
		case ADD: qop = Quad::ADD; break;
		case SUB: qop = Quad::SUB; break;
		case MUL: qop = Quad::MUL; break;
		case DIV: qop = Quad::DIV; break;
		default: assert(0);
		}
		c = env.gettemp();
		env.quads.emplace_back(qop, c, left->translate(env), right->translate(env));
	}
	return c;
}

Operand *UnaryExpr::translate(TranslateEnv &env) const
{
	Quad::Op qop;
	switch (op) {
	case NEG: qop = Quad::NEG; break;
	default: assert(0);
	}
	Operand *c = env.gettemp();
	env.quads.emplace_back(qop, c, sub->translate(env));
	return c;
}

Operand *ApplyExpr::translate(TranslateEnv &env) const
{
	ListOperand *list = new ListOperand();
	for (const unique_ptr<Expr> &arg: args)
		list->list.push_back(arg->translate(env));
	env.quads.emplace_back(Quad::CALL, func->translate(env), list);
	return getphysreg(0);
}

void EmptyStmt::translate(TranslateEnv &env) const
{
}

void CompStmt::translate(TranslateEnv &env) const
{
}

void AssignStmt::translate(TranslateEnv &env) const
{
	Operand *ovar = var->translate(env);
	Operand *oval = val->translate(env);
	env.quads.emplace_back(Quad::MOV, ovar, oval);
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
