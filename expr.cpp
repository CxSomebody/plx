#include <cassert>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "symtab.h"
#include "expr.h"

using namespace std;

void print_expr(Expr *e)
{
	switch (e->kind) {
		const char *opstr;
	case Expr::SYM:
		if (e->sym)
			printf("%s", e->sym->name.c_str());
		else
			printf("<error>");
		break;
	case Expr::LIT:
		printf("%d", e->lit);
		break;
	case Expr::COMP:
		switch (e->op) {
#define CASE(x) case Expr::x: opstr = #x; break;
			CASE(ADD)
			CASE(SUB)
			CASE(MUL)
			CASE(DIV)
			CASE(NEG)
			CASE(INDEX)
			CASE(APPLY)
#undef CASE
		default:
			assert(0);
		}
		printf("(%s ", opstr);
		if (e->left) {
			print_expr(e->left);
			putchar(' ');
		}
		print_expr(e->right);
		putchar(')');
		break;
	default:
		assert(0);
	}
}

Expr *binary_expr(Expr::Op op, Expr *left, Expr *right)
{
	Expr *e = new Expr();
	e->kind = Expr::COMP;
	e->op = op;
	e->left = left;
	e->right= right;
	return e;
};

Expr *unary_expr(Expr::Op op, Expr *right)
{
	return binary_expr(op, nullptr, right);
}

Expr *apply_expr(Expr *func, vector<Expr*> &&args)
{
	Expr *e = new Expr();
	e->kind = Expr::COMP;
	e->op = Expr::APPLY;
	e->left = func;
	// TODO memory leak
	e->args = new vector<Expr*>(std::move(args));
	return e;
}

Expr *sym_expr(Symbol *sym)
{
	Expr *e = new Expr();
	e->kind = Expr::SYM;
	e->sym = sym;
	return e;
}

Expr *ident_expr(const string &name)
{
	// TODO what if name is undefined
	return sym_expr(st->lookup(name));
}

Expr *lit_expr(int lit)
{
	Expr *e = new Expr();
	e->kind = Expr::LIT;
	e->lit = lit;
	return e;
}

Expr *index_expr(Expr *base, Expr *index)
{
	return binary_expr(Expr::INDEX, base, index);
}
