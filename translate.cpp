#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"
#include "translate.h"

using namespace std;

static const char *opstr[] = {
	"+",
	"-",
	"*",
	"/",
	"=",
	"<>",
	"<",
	">=",
	">",
	"<=",
};

int TranslateEnv::level() const
{
	return symtab->level;
}

Symbol *TranslateEnv::lookup(const std::string &name) const
{
	return symtab->lookup(name);
}

TempOperand *getphysreg(int id)
{
	static TempOperand physreg[8] =
	{{~0,4}, {~1,4}, {~2,4}, {~3,4}, {~4,4}, {~5,4}, {~6,4}, {~7,4}};
	return &physreg[id];
}

#define TODO(msg) todo(__FILE__, __LINE__, msg)

static void todo(const char *file, int line, const char *msg)
{
	fprintf(stderr, "%s: %d: TODO: %s\n", file, line, msg);
	abort();
}

void Quad::print() const
{
	switch (op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
	case Quad::DIV:
		c->print();
		printf(" = ");
		a->print();
		printf(" %s ", opstr[op]);
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
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
		printf("goto ");
		c->print();
		printf(" if ");
		a->print();
		printf(" %s ", opstr[op]);
		b->print();
		break;
	case Quad::CALL:
		printf("call ");
		c->print();
		putchar('(');
		a->print();
		putchar(')');
		break;
	case Quad::LABEL:
		c->print();
		putchar(':');
		break;
	case Quad::JMP:
		printf("goto ");
		c->print();
		break;
	case Quad::LEA:
		c->print();
		printf(" = ");
		putchar('&');
		a->print();
		break;
	default:
		assert(0);
	}
}

TempOperand *TranslateEnv::newtemp(int size)
{
	return new TempOperand(tempid++, size);
}

LabelOperand *TranslateEnv::newlabel()
{
	char tmp[16];
	sprintf(tmp, ".l%d", labelid++);
	return new LabelOperand(tmp);
}

Operand *translate_sym(Symbol *sym, TranslateEnv &env)
{
	if (sym->kind == Symbol::VAR) {
		VarSymbol *vs = static_cast<VarSymbol*>(sym);
#if 0
		if (!vs->level) {
			// global var
			return new MemOperand(new LabelOperand(vs->name));
		}
#endif
		// local var
		TempOperand *bp;
		if (vs->level == env.level()) {
			bp = getphysreg(5);
		} else {
			int leveldiff = env.level()-vs->level;
			bp = env.newtemp(4);
			env.quads.emplace_back(Quad::MOV, bp, new MemOperand(4, getphysreg(5), 8+4*(leveldiff-1)));
		}
		return new MemOperand(vs->type->size(), bp, vs->offset);
	}
	if (sym->kind == Symbol::PROC) {
		// address of function
		// TODO decorate inner function names
		return new LabelOperand(sym->name);
	}
	assert(sym->kind == Symbol::CONST);
	return new ImmOperand(static_cast<ConstSymbol*>(sym)->val);
}

Operand *SymExpr::translate(TranslateEnv &env) const
{
	return translate_sym(sym, env);
}

Operand *LitExpr::translate(TranslateEnv &env) const
{
	return new ImmOperand(lit);
}

Operand *BinaryExpr::translate(TranslateEnv &env) const
{
	Operand *c;
	Quad::Op qop;
	switch (op) {
	case ADD: qop = Quad::ADD; break;
	case SUB: qop = Quad::SUB; break;
	case MUL: qop = Quad::MUL; break;
	case DIV: qop = Quad::DIV; break;
	default: assert(0);
	}
	c = env.newtemp(type->size());
	env.quads.emplace_back(qop, c, left->translate(env), right->translate(env));
	return c;
}

Operand *IndexExpr::translate(TranslateEnv &env) const
{
	Operand *c;
	if (array->kind != SYM)
		TODO("non-SYM subscripted value");
	Symbol *arraysym = static_cast<SymExpr*>(array.get())->sym;
	if (arraysym->kind != Symbol::VAR)
		TODO("non-VAR subscripted value");
	Type *arrayty = static_cast<VarSymbol*>(arraysym)->type;
	if (arrayty->kind != Type::ARRAY)
		TODO("non-ARRAY subscripted value");
	int scale = static_cast<ArrayType*>(arrayty)->elemtype->size();
	c = array->translate(env);
	if (c->kind != Operand::MEM) {
		TODO("non-MEM subscripted value");
	}
	MemOperand *moarray = static_cast<MemOperand*>(c);
	assert(!moarray->index);
	moarray->size = type->size();
	Operand *oindex = index->translate(env);
	if (oindex->kind == Operand::MEM) {
		TempOperand *tmp = env.newtemp(index->type->size());
		env.quads.emplace_back(Quad::MOV, tmp, oindex);
		oindex = tmp;
	}
	if (oindex->kind == Operand::IMM) {
		moarray->offset += static_cast<ImmOperand*>(oindex)->val * scale;
	} else if (oindex->kind == Operand::TEMP) {
		moarray->index = static_cast<TempOperand*>(oindex);
		moarray->scale = scale;
	} else {
		assert(0);
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
	Operand *c = env.newtemp(type->size());
	env.quads.emplace_back(qop, c, sub->translate(env));
	return c;
}

Operand *ApplyExpr::translate(TranslateEnv &env) const
{
	ListOperand *list = new ListOperand();
	for (const unique_ptr<Expr> &arg: args)
		list->list.push_back(arg->translate(env));
	env.quads.emplace_back(Quad::CALL, translate_sym(func, env), list);
	return getphysreg(0);
}

void EmptyStmt::translate(TranslateEnv &env) const
{
	// do nothing
}

void CompStmt::translate(TranslateEnv &env) const
{
	for (const unique_ptr<Stmt> &s: body)
		s->translate(env);
}

void AssignStmt::translate(TranslateEnv &env) const
{
	Operand *ovar;
	Symbol *varsym;
	if (var->kind == Expr::SYM &&
	    (varsym = static_cast<SymExpr*>(var.get())->sym)->kind == Symbol::PROC) {
		ovar = translate_sym(env.lookup(varsym->name+'$'), env);
	} else {
		ovar = var->translate(env);
	}
	Operand *oval = val->translate(env);
	env.quads.emplace_back(Quad::MOV, ovar, oval);
}

void CallStmt::translate(TranslateEnv &env) const
{
	ListOperand *list = new ListOperand();
	for (const unique_ptr<Expr> &arg: args)
		list->list.push_back(arg->translate(env));
	env.quads.emplace_back(Quad::CALL, translate_sym(proc, env), list);
}

void IfStmt::translate(TranslateEnv &env) const
{
	LabelOperand *lfalse, *ljoin;
	lfalse = env.newlabel();
	cond->translate(env, lfalse, true);
	if (sf)
		ljoin = env.newlabel();
	st->translate(env);
	if (sf)
		env.quads.emplace_back(Quad::JMP, ljoin);
	env.quads.emplace_back(Quad::LABEL, lfalse);
	if (sf) {
		sf->translate(env);
		env.quads.emplace_back(Quad::LABEL, ljoin);
	}
}

void WhileStmt::translate(TranslateEnv &env) const
{
	LabelOperand *lstart = env.newlabel();
	LabelOperand *lend = env.newlabel();
	env.quads.emplace_back(Quad::LABEL, lstart);
	cond->translate(env, lend, true);
	body->translate(env);
	env.quads.emplace_back(Quad::JMP, lstart);
	env.quads.emplace_back(Quad::LABEL, lend);
}

void DoWhileStmt::translate(TranslateEnv &env) const
{
	LabelOperand *lstart = env.newlabel();
	env.quads.emplace_back(Quad::LABEL, lstart);
	body->translate(env);
	cond->translate(env, lstart, false);
}

void ForStmt::translate(TranslateEnv &env) const
{
	// indvar = from;
	Operand *o_indvar = indvar->translate(env);
	env.quads.emplace_back(Quad::MOV, o_indvar, from->translate(env));
	// lim = to;
	TempOperand *lim = env.newtemp(indvar->type->size());
	env.quads.emplace_back(Quad::MOV, lim, to->translate(env));
	// while (indvar <= lim) {
	// 	<stmt>;
	// 	indvar++;
	// }
	LabelOperand *lstart = env.newlabel();
	LabelOperand *lend = env.newlabel();
	env.quads.emplace_back(Quad::LABEL, lstart);
	env.quads.emplace_back(Quad::BGT, lend, o_indvar, lim);
	// <cond>
	body->translate(env);
	env.quads.emplace_back(Quad::JMP, lstart);
	env.quads.emplace_back(Quad::LABEL, lend);
}

void ReadStmt::translate(TranslateEnv &env) const
{
	static LabelOperand o_scanf("scanf");
	for (const unique_ptr<Expr> &var: vars) {
		Operand *o = var->translate(env);
		if (o->kind != Operand::MEM) {
			TODO("expected lvalue in read stmt");
		}
		Operand *addr = env.newtemp(4);
		env.quads.emplace_back(Quad::LEA, addr, o);
		Operand *fmtstr;
		if (var->type == int_type())
			fmtstr = new LabelOperand("$fmtd");
		else if (var->type == char_type())
			fmtstr = new LabelOperand("$fmtc");
		else
			assert(0);
		ListOperand *args = new ListOperand();
		args->list.push_back(fmtstr);
		args->list.push_back(addr);
		env.quads.emplace_back(Quad::CALL, &o_scanf, args);
	}
}

void WriteStmt::translate(TranslateEnv &env) const
{
	static LabelOperand o_printf("printf");
	Operand *fmtstr;
	ListOperand *args;
#if 0
	if (!str.empty()) {
		fmtstr = new LabelOperand("$fmts_");
		args = new ListOperand();
		args->list.push_back(fmtstr);
		args->list.push_back(new LabelOperand(/*TODO*/));
		env.quads.emplace_back(Quad::CALL, &o_printf, args);
	}
#endif
	if (val) {
		if (val->type == int_type())
			fmtstr = new LabelOperand("$fmtd");
		else if (val->type == char_type())
			fmtstr = new LabelOperand("$fmtc");
		else
			assert(0);
		args = new ListOperand();
		args->list.push_back(fmtstr);
		args->list.push_back(val->translate(env));
		env.quads.emplace_back(Quad::CALL, &o_printf, args);
	}
}

void Cond::translate(TranslateEnv &env, LabelOperand *label, bool negate) const
{
	Quad::Op qop;
	switch (op^negate) {
	case Cond::EQ: qop = Quad::BEQ; break;
	case Cond::NE: qop = Quad::BNE; break;
	case Cond::LT: qop = Quad::BLT; break;
	case Cond::GE: qop = Quad::BGE; break;
	case Cond::GT: qop = Quad::BGT; break;
	case Cond::LE: qop = Quad::BLE; break;
	default: assert(0);
	}
	env.quads.emplace_back(qop, label, left->translate(env), right->translate(env));
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
	env.gencode();
	printf("end %s\n", name.c_str());
}

void translate_all(unique_ptr<Block> &&blk)
{
	blk->translate();
	blk->print(0);
}
