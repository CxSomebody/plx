#include <cassert>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "symtab.h"
#include "expr.h"

using namespace std;

void print_symbol(Symbol *s)
{
	if (s) printf("%s", s->name.c_str());
	else printf("<error>");
}

void print_expr(Expr *e)
{
	switch (e->kind) {
		const char *opstr;
	case Expr::SYM:
		print_symbol(e->sym);
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
		if (e->op == Expr::APPLY) {
			bool sep = false;
			for (Expr *arg: *e->args) {
				if (sep)
					putchar(' ');
				print_expr(arg);
				sep = true;
			}
		} else {
			print_expr(e->right);
		}
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

Expr *apply_expr(Expr *func, const vector<Expr*> &args)
{
	Expr *e = new Expr();
	e->kind = Expr::COMP;
	e->op = Expr::APPLY;
	e->left = func;
	// TODO memory leak
	e->args = new vector<Expr*>(args);
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
	return sym_expr(symtab->lookup(name));
}

Expr *lit_expr(int lit)
{
	Expr *e = new Expr();
	e->kind = Expr::LIT;
	e->lit = lit;
	return e;
}

void print_stmt(Stmt *s)
{
	if (!s) {
		printf("<null>");
		return;
	}
	switch (s->kind) {
		bool sep;
	case Stmt::ASSIGN:
		print_expr(s->var);
		printf(" := ");
		print_expr(s->val);
		break;
	case Stmt::CALL:
		print_symbol(s->proc);
		putchar('(');
		sep = false;
		for (Expr *arg: *s->args) {
			if (sep)
				printf(", ");
			print_expr(arg);
			sep = true;
		}
		putchar(')');
		break;
	case Stmt::IF:
		printf("IF ");
		print_cond(s->cond);
		printf(" THEN ");
		print_stmt(s->st);
		printf(" ELSE ");
		print_stmt(s->sf);
		break;
	case Stmt::COMP:
		putchar('(');
		sep = false;
		for (Stmt *t: *s->body) {
			if (sep)
				printf("; ");
			print_stmt(t);
			sep = true;
		}
		putchar(')');
		break;
	default:
		assert(0);
	};
}

Stmt *assign_stmt(Expr *var, Expr *val)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::ASSIGN;
	s->var = var;
	s->val = val;
	print_stmt(s);
	putchar(10);
	return s;
}

Stmt *call_stmt(const string &name, const vector<Expr*> &args)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::CALL;
	s->proc = symtab->lookup(name);
	// TODO memory leak
	s->args = new vector<Expr*>(args);
	print_stmt(s);
	putchar(10);
	return s;
}

Stmt *if_stmt(Cond *cond, Stmt *st, Stmt *sf)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::IF;
	s->cond = cond;
	s->st = st;
	s->sf = sf;
	print_stmt(s);
	putchar(10);
	return s;
}

Stmt *if_stmt(Cond *cond, Stmt *st)
{
	return if_stmt(cond, st, nullptr);
}

Stmt *comp_stmt(const std::vector<Stmt*> &body)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::COMP;
	s->body = new vector<Stmt*>(body);
	return s;
}

void print_cond(Cond *c)
{
	const char *opstr;
	switch (c->op) {
	case Cond::EQ: opstr = "=" ; break;
	case Cond::NE: opstr = "<>"; break;
	case Cond::LT: opstr = "<" ; break;
	case Cond::GE: opstr = ">="; break;
	case Cond::GT: opstr = ">" ; break;
	case Cond::LE: opstr = "<="; break;
	default: assert(0);
	}
	print_expr(c->left);
	printf(" %s ", opstr);
	print_expr(c->right);
}

Cond *cond(Cond::Op op, Expr *left, Expr *right)
{
	Cond *c = new Cond();
	c->op = op;
	c->left = left;
	c->right= right;
	return c;
}
