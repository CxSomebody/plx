#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "symtab.h"
#include "expr.h"

using namespace std;

Expr::Expr(Expr::Kind kind): kind(kind) {}
SymExpr::SymExpr(Symbol *sym): Expr(SYM), sym(sym) {}
LitExpr::LitExpr(int lit): Expr(LIT), lit(lit) {}
BinaryExpr::BinaryExpr(Expr *left, Expr *right): Expr(BINARY), left(left), right(right) {}
UnaryExpr::UnaryExpr(Expr *sub): Expr(UNARY), sub(sub) {}
ApplyExpr::ApplyExpr(Expr *func, vector<unique_ptr<Expr>> &&args): Expr(APPLY), func(func), args(move(args)) {}

void SymExpr::print()
{
	sym->print();
}

void LitExpr::print()
{
	printf("%d", lit);
}

void BinaryExpr::print()
{
	if (op == BinaryExpr::INDEX) {
		left->print();
		putchar('[');
		right->print();
		putchar(']');
	} else {
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
}

void UnaryExpr::print()
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

void ApplyExpr::print()
{
	func->print();
	print_expr_list(args);
}

Expr *ident_expr(const string &name)
{
	return new SymExpr(symtab->lookup(name));
}

Stmt::Stmt(Kind kind): kind(kind) {}
EmptyStmt::EmptyStmt(): Stmt(EMPTY) {}
CompStmt::CompStmt(vector<unique_ptr<Stmt>> &&body): Stmt(COMP), body(move(body)) {}
AssignStmt::AssignStmt(Expr *var, Expr *val): Stmt(ASSIGN), var(var), val(val) {}
CallStmt::CallStmt(vector<unique_ptr<Expr>> &&args): Stmt(CALL), args(move(args)) {}
IfStmt::IfStmt(Cond *cond, Stmt *st, Stmt *sf): Stmt(IF), cond(cond), st(st), sf(sf) {}
DoWhileStmt::DoWhileStmt(Cond *cond, Stmt *body): Stmt(DO_WHILE), cond(cond), body(body) {}
ForStmt::ForStmt(Expr *indvar, Expr *from, Expr *to, Stmt *body, bool down): Stmt(FOR), indvar(indvar), from(from), to(to), body(body), down(down) {}
ReadStmt::ReadStmt(vector<unique_ptr<Expr>> &&vars): Stmt(READ), vars(move(vars)) {}
WriteStmt::WriteStmt(const string &str, Expr *val): Stmt(WRITE), str(str), val(val) {}

void EmptyStmt::print()
{
}

void CompStmt::print()
{
	bool sep;
	printf("begin ");
	for (auto &s: body) {
		if (sep)
			printf("; ");
		s->print();
		sep = true;
	}
	printf(" end");
}

void AssignStmt::print()
{
	var->print();
	printf(" := ");
	val->print();
}

void CallStmt::print()
{
	proc->print();
	putchar('(');
	print_expr_list(args);
	putchar(')');
}

void IfStmt::print()
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

void DoWhileStmt::print()
{
	printf("do ");
	body->print();
	printf(" while ");
	cond->print();
}

void ForStmt::print()
{
	printf("for ");
	indvar->print();
	printf(" := ");
	from->print();
	printf(down ? " DOWNTO " : " TO ");
	to->print();
	printf(" do ");
	body->print();
}

void ReadStmt::print()
{
	printf("read");
	print_expr_list(vars);
}

void WriteStmt::print()
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

void Cond::print()
{
	const char *opstr;
	switch (op) {
	case Cond::EQ: opstr = "=" ; break;
	case Cond::NE: opstr = "<>"; break;
	case Cond::LT: opstr = "<" ; break;
	case Cond::GE: opstr = ">="; break;
	case Cond::GT: opstr = ">" ; break;
	case Cond::LE: opstr = "<="; break;
	default: assert(0);
	}
	left->print();
	printf(" %s ", opstr);
	right->print();
}

Cond::Cond(Op op, Expr *left, Expr *right): op(op), left(left), right(right) {}
