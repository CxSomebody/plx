#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "semant.h"

using namespace std;

Expr::~Expr() {}
Expr::Expr(Kind kind, Type *type): kind(kind), type(type) {}
SymExpr::SymExpr(Symbol *sym): Expr(SYM, sym->type), sym(sym) {}
LitExpr::LitExpr(int lit): Expr(LIT, int_type()), lit(lit) {}
BinaryExpr::BinaryExpr(Op op, unique_ptr<Expr> &&left, unique_ptr<Expr> &&right): Expr(BINARY, binexprtype(left->type, right->type)), op(op), left(move(left)), right(move(right))
{
	//"operation <TODO> not defined for operand types %s and %s"
}
UnaryExpr::UnaryExpr(Op op, unique_ptr<Expr> &&sub): Expr(UNARY, sub->type), op(op), sub(move(sub)) {}
ApplyExpr::ApplyExpr(ProcSymbol *func, vector<unique_ptr<Expr>> &&args): Expr(APPLY, func->rettype), func(func), args(move(args)) {}

void SymExpr::print() const
{
	sym->print();
}

void LitExpr::print() const
{
	printf("%d", lit);
}

void BinaryExpr::print() const
{
	char opchar;
	switch (op) {
#define CASE(x,y) case BinaryExpr::x: opchar = y; break
		CASE(ADD,'+');
		CASE(SUB,'-');
		CASE(MUL,'*');
		CASE(DIV,'/');
#undef CASE
	default:
		assert(0);
	}
	putchar('(');
	left->print();
	putchar(opchar);
	right->print();
	putchar(')');
}

void IndexExpr::print() const
{
	array->print();
	putchar('[');
	index->print();
	putchar(']');
}

void UnaryExpr::print() const
{
	const char *opstr;
	switch (op) {
#define CASE(x) case UnaryExpr::x: opstr = #x; break;
		CASE(NEG)
#undef CASE
	default:
		assert(0);
	}
	printf("(%s ", opstr);
	sub->print();
	putchar(')');
}

void print_expr_list(const vector<unique_ptr<Expr>> &list)
{
	bool sep = false;
	putchar('(');
	for (auto &e: list) {
		if (sep)
			printf(", ");
		e->print();
		sep = true;
	}
	putchar(')');
}

void ApplyExpr::print() const
{
	func->print();
	print_expr_list(args);
}

Stmt::~Stmt() {}
Stmt::Stmt(Kind kind): kind(kind) {}
EmptyStmt::EmptyStmt(): Stmt(EMPTY) {}
CompStmt::CompStmt(vector<unique_ptr<Stmt>> &&body): Stmt(COMP), body(move(body)) {}
AssignStmt::AssignStmt(unique_ptr<Expr> &&var, unique_ptr<Expr> &&val): Stmt(ASSIGN), var(move(var)), val(move(val)) {}
CallStmt::CallStmt(ProcSymbol *proc, vector<unique_ptr<Expr>> &&args): Stmt(CALL), proc(proc), args(move(args)) {}
IfStmt::IfStmt(unique_ptr<Cond> &&cond, unique_ptr<Stmt> &&st): Stmt(IF), cond(move(cond)), st(move(st)) {}
IfStmt::IfStmt(unique_ptr<Cond> &&cond, unique_ptr<Stmt> &&st, unique_ptr<Stmt> &&sf): Stmt(IF), cond(move(cond)), st(move(st)), sf(move(sf)) {}
WhileStmt::WhileStmt(unique_ptr<Cond> &&cond, unique_ptr<Stmt> &&body): Stmt(DO_WHILE), cond(move(cond)), body(move(body)) {}
DoWhileStmt::DoWhileStmt(unique_ptr<Cond> &&cond, unique_ptr<Stmt> &&body): Stmt(DO_WHILE), cond(move(cond)), body(move(body)) {}
ForStmt::ForStmt(unique_ptr<Expr> &&indvar, unique_ptr<Expr> &&from, unique_ptr<Expr> &&to, unique_ptr<Stmt> &&body, bool down): Stmt(FOR), indvar(move(indvar)), from(move(from)), to(move(to)), body(move(body)), down(down) {}
ReadStmt::ReadStmt(vector<unique_ptr<Expr>> &&vars): Stmt(READ), vars(move(vars)) {}
WriteStmt::WriteStmt(string &&str, unique_ptr<Expr> &&val): Stmt(WRITE), str(move(str)), val(move(val)) {}

void EmptyStmt::print() const
{
}

void CompStmt::print() const
{
	bool sep = false;
	printf("begin ");
	for (auto &s: body) {
		if (sep)
			printf("; ");
		s->print();
		sep = true;
	}
	printf(" end");
}

void AssignStmt::print() const
{
	var->print();
	printf(" := ");
	val->print();
}

void CallStmt::print() const
{
	proc->print();
	print_expr_list(args);
}

void IfStmt::print() const
{
	printf("if ");
	cond->print();
	printf(" then ");
	st->print();
	if (sf) {
		printf(" else ");
		sf->print();
	}
}

void WhileStmt::print() const
{
	printf("while ");
	cond->print();
	printf(" do ");
	body->print();
}

void DoWhileStmt::print() const
{
	printf("do ");
	body->print();
	printf(" while ");
	cond->print();
}

void ForStmt::print() const
{
	printf("for ");
	indvar->print();
	printf(" := ");
	from->print();
	printf(down ? " downto " : " to ");
	to->print();
	printf(" do ");
	body->print();
}

void ReadStmt::print() const
{
	printf("read");
	print_expr_list(vars);
}

void WriteStmt::print() const
{
	printf("write");
	putchar('(');
	printf("\"%s\"", str.c_str());
	if (val) {
		printf(", ");
		val->print();
	}
	putchar(')');
}

void SimpleCond::print() const
{
	const char *opstr;
	switch (op) {
	case EQ: opstr = "=" ; break;
	case NE: opstr = "<>"; break;
	case LT: opstr = "<" ; break;
	case GE: opstr = ">="; break;
	case GT: opstr = ">" ; break;
	case LE: opstr = "<="; break;
	default: assert(0);
	}
	left->print();
	printf(" %s ", opstr);
	right->print();
}

void CompCond::print() const
{
	const char *opstr;
	switch (op) {
	case AND: opstr = "and"; break;
	case OR : opstr = "or" ; break;
	default: assert(0);
	}
	left->print();
	printf(" %s ", opstr);
	right->print();
}
