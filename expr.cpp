#include <cassert>
#include <cstdlib>
#include <cstring>
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
	case Stmt::COMP:
		putchar('(');
		sep = false;
		for (Stmt *t: *s->comp.body) {
			if (sep)
				printf("; ");
			print_stmt(t);
			sep = true;
		}
		putchar(')');
		break;
	case Stmt::ASSIGN:
		print_expr(s->assign.var);
		printf(" := ");
		print_expr(s->assign.val);
		break;
	case Stmt::CALL:
		print_symbol(s->call.proc);
		putchar('(');
		sep = false;
		for (Expr *arg: *s->call.args) {
			if (sep)
				printf(", ");
			print_expr(arg);
			sep = true;
		}
		putchar(')');
		break;
	case Stmt::IF:
		printf("IF ");
		print_cond(s->if_.cond);
		printf(" THEN ");
		print_stmt(s->if_.st);
		printf(" ELSE ");
		print_stmt(s->if_.sf);
		break;
	case Stmt::DO_WHILE:
		printf("DO ");
		print_stmt(s->do_while.body);
		printf(" WHILE ");
		print_cond(s->do_while.cond);
		break;
	case Stmt::FOR:
		printf("FOR ");
		print_expr(s->for_.indvar);
		printf(" := ");
		print_expr(s->for_.from);
		printf(s->for_.down ? " DOWNTO " : " TO ");
		print_expr(s->for_.to);
		printf(" DO ");
		print_stmt(s->for_.body);
		break;
	case Stmt::READ:
		printf("READ ");
		sep = false;
		for (Expr *e: *s->read.vars) {
			if (sep)
				printf(", ");
			print_expr(e);
			sep = true;
		}
		break;
	case Stmt::WRITE:
		printf("WRITE \"%s\"", s->write.str);
		if (s->write.val) {
			putchar(' ');
			print_expr(s->write.val);
		}
		break;
	default:
		assert(0);
	};
}

Stmt *assign_stmt(Expr *var, Expr *val)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::ASSIGN;
	s->assign.var = var;
	s->assign.val = val;
	return s;
}

Stmt *call_stmt(const string &name, const vector<Expr*> &args)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::CALL;
	s->call.proc = symtab->lookup(name);
	s->call.args = new vector<Expr*>(args);
	return s;
}

Stmt *if_stmt(Cond *cond, Stmt *st, Stmt *sf)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::IF;
	s->if_.cond = cond;
	s->if_.st = st;
	s->if_.sf = sf;
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
	s->comp.body = new vector<Stmt*>(body);
	return s;
}

Stmt *do_while_stmt(Cond *cond, Stmt *body)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::DO_WHILE;
	s->do_while.cond = cond;
	s->do_while.body = body;
	return s;
}

Stmt *for_stmt(Expr *indvar, Expr *from, Expr *to, Stmt *body, bool down)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::FOR;
	s->for_.indvar = indvar;
	s->for_.from = from;
	s->for_.to = to;
	s->for_.body = body;
	s->for_.down = down;
	return s;
}

Stmt *read_stmt(const std::vector<Expr*> &vars)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::READ;
	s->read.vars = new vector<Expr*>(vars);
	return s;
}

Stmt *write_stmt(const std::string &str, Expr *val)
{
	Stmt *s = new Stmt();
	s->kind = Stmt::WRITE;
	s->write.str = strdup(str.c_str());
	s->write.val = val;
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
