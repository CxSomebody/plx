#include <cstdarg>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

extern "C" {
#include "lexer.h"
#include "tokens.h"
}

using namespace std;

static struct Token {
	int sym;
	int line, col;
	string spell;
	int i;
	string s;
} tok, ntok;

// (syncsym, level)
vector<pair<int, int>> sync;
// number of synchronizing tokens for each level
vector<int> nsync;

struct X {
	X(initializer_list<int> &&il) {
		int n = il.size();
		nsync.push_back(n);
		//printf("enter level %d (%d)\n", level, n);
		int level = nsync.size();
		for (int s: il)
			sync.emplace_back(s, level);
	}
	~X() {
		int n = nsync.back();
		nsync.pop_back();
		//int level = nsync.size();
		//printf("leave level %d (%d)\n", level, n);
		sync.erase(sync.end()-n, sync.end());
	}
};

// both syntactic and semantic
int parser_errors;

static void savetok(int s, Token *t)
{
	t->sym = s;
	t->line = lineno;
	t->col = colno;
	t->spell = string(tokstart, toklen);
	if (s == INT || s == CHAR)
		t->i = tokval.i;
	else if (s == IDENT || s == STRING)
		t->s = tokval.s;
}

static void getsym()
{
	tok = move(ntok);
	savetok(lex(), &ntok);
}

static void error(const char *fmt...)
{
	parser_errors++;
	fprintf(stderr, "%s:%d:%d: ", fpath, tok.line, tok.col);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void recover()
{
	for (;;) {
		for (auto it = sync.rbegin();
		     it != sync.rend();
		     it++)
			if (tok.sym == it->first)
				throw it->second;
		getsym();
	}
}

static void expecting(int s)
{
	char tmp[4];
	const char *sname;
	if (s<256) {
		if (s) {
			tmp[0] = '\'';
			tmp[1] = s;
			tmp[2] = '\'';
			tmp[3] = 0;
			sname = tmp;
		} else {
			sname = "EOF";
		}
	} else {
		sname = tokname[s-256];
	}
	error("expected %s, found ‘%s’", sname, tok.spell.c_str());
}

static void unexp()
{
	error("unexpected ‘%s’", tok.spell.c_str());
}

// returns true upon ERROR
static void check(int s)
{
	if (tok.sym != s) {
		expecting(s);
		recover();
	}
}

static void missing(int s)
{
	error("missing '%c' before ‘%s’", s, tok.spell.c_str());
}

static void extra()
{
	error("extraneous token ‘%s’", tok.spell.c_str());
	getsym();
}

static unique_ptr<Block> program();
static unique_ptr<Block> block(ProcSymbol *proc);
static void const_part();
static void const_def();
static int constant();
static bool opt_sign();
static vector<VarSymbol*> var_part();
static void var_decl(vector<VarSymbol*> &list);
static vector<string> id_list();
static Type *type();
static Type *basic_type();
static vector<unique_ptr<Block>> sub_list();
static ProcSymbol *proc_header(bool isfunc);
static vector<Param> param_list();
static vector<Param> param_group();
static unique_ptr<Stmt> stmt();
static unique_ptr<CompStmt> comp_stmt();
static unique_ptr<AssignStmt> assign_stmt();
static unique_ptr<CallStmt> call_stmt();
static unique_ptr<IfStmt> if_stmt();
static unique_ptr<WhileStmt> while_stmt();
static unique_ptr<DoWhileStmt> do_while_stmt();
static unique_ptr<ForStmt> for_stmt();
static unique_ptr<ReadStmt> read_stmt();
static unique_ptr<WriteStmt> write_stmt();
static unique_ptr<Expr> lvalue();
static unique_ptr<Expr> factor();
static unique_ptr<Expr> term();
static unique_ptr<Expr> expr();
static vector<unique_ptr<Expr>> expr_list();
static unique_ptr<Cond> cond();
static unique_ptr<Cond> disjunction();
static unique_ptr<Cond> conjunction();
static unique_ptr<Cond> primary_cond();

Symbol *lookup_checked(const string &name)
{
	Symbol *s = lookup(name);
	if (!s)
		error("‘%s’ is undefined", tok.spell.c_str());
	return s;
}

unique_ptr<Expr> ident_expr(const string &name)
{
	Symbol *s = lookup_checked(name);
	return s ? make_unique<SymExpr>(lookup(name)) : nullptr;
}

static void checkprocsym(Symbol *s)
{
	if (s->kind != Symbol::PROC)
		error("‘%s’ is not a proc symbol", tok.s.c_str());
}

unique_ptr<Block> parse()
{
	savetok(lex(), &tok);
	savetok(lex(), &ntok);
	return program();
}

#define CATCH \
	catch (int l) { \
		int level = nsync.size(); \
		if (l != level) \
			throw l; \
	}

#define CATCH_R(E) \
	catch (int l) { \
		int level = nsync.size(); \
		if (l != level) \
			throw l; \
		return E; \
	}

static unique_ptr<Block> program()
{
	X _{0};
	try {
		unique_ptr<Block> blk(block(nullptr));
		check('.'); getsym();
		check(0);
		return blk;
	} CATCH_R(nullptr)
}

static unique_ptr<Block> block(ProcSymbol *proc)
{
	X _{';', '.'};
	try {
		push_symtab(proc);
		if (proc)
			def_params(proc->params);
		if (tok.sym == T_CONST) {
			getsym();
			const_part();
		}
		vector<VarSymbol*> vars;
		if (tok.sym == T_VAR) {
			getsym();
			vars = var_part();
		}
		if (proc && proc->rettype) {
			// local var for function return value
			vars.push_back(def_var(proc->name+'$', proc->rettype));
		}
		vector<unique_ptr<Block>> subs(sub_list());
		unique_ptr<CompStmt> body(comp_stmt());
		SymbolTable *symtab = pop_symtab();
		return make_unique<Block>
			(proc,
			 move(subs),
			 move(vars),
			 move(body->body),
			 symtab);
	} CATCH_R(nullptr)
}

static void const_part()
{
	X _{T_VAR, T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		for (;;) {
			const_def();
			if (tok.sym != ',') {
				switch (tok.sym) {
				case ';':
					getsym();
					break;
				case T_VAR:
				case T_PROCEDURE:
				case T_FUNCTION:
				case T_BEGIN:
					missing(';');
					break;
				default:
					expecting(';');
				}
				break;
			}
			getsym();
		}
	} CATCH
}

static void const_def()
{
	X _{',', ';'};
	try {
		check(IDENT);
		string name(move(tok.s));
		getsym();
		check('='); getsym();
		int val = constant();
		def_const(name, val);
	} CATCH
}

static int constant()
{
	X _{',', ';'};
	try {
		int ret;
		if (tok.sym == CHAR) {
			ret = tok.i;
			getsym();
		} else {
			bool neg = opt_sign();
			check(INT);
			ret = tok.i;
			getsym();
			if (neg)
				ret = -ret;
		}
		return ret;
	} CATCH_R(0)
}

static bool opt_sign()
{
	if (tok.sym == '-') {
		getsym();
		return true;
	}
	if (tok.sym == '+')
		getsym();
	return false;
}

static vector<VarSymbol*> var_part()
{
	X _{T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		vector<VarSymbol*> ret;
		do {
			var_decl(ret);
			switch (tok.sym) {
			case ';':
				getsym();
				break;
			case T_PROCEDURE:
			case T_FUNCTION:
			case T_BEGIN:
				missing(';');
				break;
			default:
				expecting(';');
			}
		} while (tok.sym == IDENT);
		return ret;
	} CATCH_R((vector<VarSymbol*>()))
}

static void var_decl(vector<VarSymbol*> &list)
{
	X _{';'};
	try {
		vector<string> names(id_list());
		check(':'); getsym();
		Type *ty = type();
		for (const string &name: names) {
			list.emplace_back(def_var(name, ty));
		}
	} CATCH
}

static vector<string> id_list()
{
	X _{':'};
	try {
		vector<string> ret;
		for (;;) {
			check(IDENT);
			ret.emplace_back(move(tok.s));
			getsym();
			if (tok.sym != ',')
				break;
			getsym();
		}
		return ret;
	} CATCH_R(vector<string>())
}

static Type *type()
{
	X _{';'};
	try {
		if (tok.sym == T_ARRAY) {
			getsym();
			check('['); getsym();
			check(INT);
			int n = tok.i;
			getsym();
			check(']'); getsym();
			check(T_OF); getsym();
			return array_type(basic_type(), n);
		}
		return basic_type();
	} CATCH_R(nullptr)
}

static Type *basic_type()
{
	X _{';', ')'};
	try {
		if (tok.sym == T_INTEGER) {
			getsym();
			return int_type();
		}
		check(T_CHAR); getsym();
		return char_type();
	} CATCH_R(nullptr)
}

static vector<unique_ptr<Block>> sub_list()
{
	X _{T_BEGIN};
	try {
		vector<unique_ptr<Block>> ret;
		for (;;) {
			bool isfunc = tok.sym == T_FUNCTION;
			if (tok.sym == T_PROCEDURE || isfunc) {
				getsym();
				ProcSymbol *proc = proc_header(isfunc);
				switch (tok.sym) {
				case ';':
					getsym();
					break;
				case T_CONST:
				case T_VAR:
				case T_PROCEDURE:
				case T_FUNCTION:
				case T_BEGIN:
					missing(';');
					break;
				default:
					expecting(';');
				}
				ret.emplace_back(block(proc));
				switch (tok.sym) {
				case ';':
					getsym();
					break;
				case T_PROCEDURE:
				case T_FUNCTION:
				case T_BEGIN:
					missing(';');
					break;
				default:
					expecting(';');
				}
			} else {
				return ret;
			}
		}
	} CATCH_R(vector<unique_ptr<Block>>())
}

static ProcSymbol *proc_header(bool isfunc)
{
	X _{T_CONST, T_VAR, T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		check(IDENT);
		string name(move(tok.s));
		getsym();
		vector<Param> params;
		if (tok.sym == '(') {
			getsym();
			params = param_list();
			check(')'); getsym();
		}
		Type *retty = nullptr;
		if (isfunc) {
			switch (tok.sym) {
			case ':':
				getsym();
				retty = basic_type();
				break;
			case ';':
				error("missing return type declaration");
				break;
			default:
				unexp();
				recover();
			}
		}
		return def_func(name, params, retty);
	} CATCH_R(nullptr)
}

static vector<Param> param_list()
{
	X _{')'};
	try {
		vector<Param> ret;
		for (;;) {
			for (Param &p: param_group())
				ret.emplace_back(move(p));
			if (tok.sym != ';')
				break;
			getsym();
		}
		return ret;
	} CATCH_R(vector<Param>())
}

static vector<Param> param_group()
{
	X _{';', ')'};
	try {
		vector<Param> ret;
		bool byref = false;
		if (tok.sym == T_VAR) {
			getsym();
			byref = true;
		}
		vector<string> names(id_list());
		check(':'); getsym();
		Type *ty = basic_type();
		return param_group(move(names), ty, byref);
	} CATCH_R(vector<Param>())
}

static unique_ptr<CompStmt> comp_stmt()
{
	X _{';', T_PROCEDURE, T_FUNCTION, T_BEGIN, T_END, T_ELSE, T_WHILE, '.'};
	try {
		vector<unique_ptr<Stmt>> stmts;
		check(T_BEGIN); getsym();
		for (;;) {
			stmts.emplace_back(stmt());
			if (tok.sym == T_END)
				break;
			switch (tok.sym) {
			case ';':
				getsym();
				break;
			case IDENT:
			case T_BEGIN:
			case T_IF:
			case T_DO:
			case T_FOR:
			case T_READ:
			case T_WRITE:
				missing(';');
				break;
			default:
				expecting(';');
				// we may get stuck in an infinite loop otherwise, because <stmt> is nullable
				getsym();
			}
		}
		check(T_END); getsym();
		return make_unique<CompStmt>(move(stmts));
	} CATCH_R(nullptr)
}

static unique_ptr<Stmt> stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		switch (tok.sym) {
		case IDENT:
			if (ntok.sym == BECOMES ||
			    ntok.sym == '=' /* error */ ||
			    ntok.sym == '[')
				return assign_stmt();
			return call_stmt();
		case T_BEGIN:
			return comp_stmt();
		case T_IF:
			return if_stmt();
		case T_WHILE:
			return while_stmt();
		case T_DO:
			return do_while_stmt();
		case T_FOR:
			return for_stmt();
		case T_READ:
			return read_stmt();
		case T_WRITE:
			return write_stmt();
		}
		return make_unique<EmptyStmt>();
	} CATCH_R(nullptr)
}

static unique_ptr<AssignStmt> assign_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		unique_ptr<Expr> var(lvalue());
		if (tok.sym == BECOMES) {
			getsym();
		} else if (tok.sym == '=') {
			error("use ‘:=’ for assignment, not ‘=’");
			getsym();
		} else {
			unexp();
			recover();
		}
		unique_ptr<Expr> val(expr());
		return make_unique<AssignStmt>(move(var), move(val));
	} CATCH_R(nullptr)
}

static unique_ptr<CallStmt> call_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(IDENT);
		Symbol *proc = lookup_checked(tok.s);
		if (proc)
			checkprocsym(proc);
		getsym();
		vector<unique_ptr<Expr>> args;
		if (tok.sym == '(') {
			check('('); getsym();
			args = expr_list();
			check(')'); getsym();
		}
		return proc ? make_unique<CallStmt>(static_cast<ProcSymbol*>(proc), move(args)) : nullptr;
	} CATCH_R(nullptr)
}

static unique_ptr<IfStmt> if_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(T_IF); getsym();
		unique_ptr<Cond> c(cond());
		check(T_THEN); getsym();
		unique_ptr<Stmt> st(stmt());
		if (tok.sym == ';' && ntok.sym == T_ELSE) {
			extra();
			goto else_part;
		}
		if (tok.sym == T_ELSE) {
else_part:
			getsym();
			return make_unique<IfStmt>(move(c), move(st), stmt());
		}
		return make_unique<IfStmt>(move(c), move(st), nullptr);
	} CATCH_R(nullptr)
}

static unique_ptr<WhileStmt> while_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(T_WHILE); getsym();
		unique_ptr<Cond> c(cond());
		check(T_DO); getsym();
		unique_ptr<Stmt> body(stmt());
		return make_unique<WhileStmt>(move(c), move(body));
	} CATCH_R(nullptr)
}

static unique_ptr<DoWhileStmt> do_while_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(T_DO); getsym();
		unique_ptr<Stmt> body(stmt());
		if (tok.sym == T_WHILE) {
			getsym();
		} else if (tok.sym == ';' && ntok.sym == T_WHILE) {
			extra();
			getsym();
		} else {
			expecting(T_WHILE);
		}
		unique_ptr<Cond> c(cond());
		return make_unique<DoWhileStmt>(move(c), move(body));
	} CATCH_R(nullptr)
}

static unique_ptr<ForStmt> for_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		bool down = false;
		check(T_FOR); getsym();
		unique_ptr<Expr> ind(lvalue());
		check(BECOMES); getsym();
		unique_ptr<Expr> from(expr());
		switch (tok.sym) {
		case T_DOWNTO:
			down = true;
			// fallthrough
		case T_TO:
			break;
		default:
			unexp();
			recover();
		}
		getsym();
		unique_ptr<Expr> to(expr());
		check(T_DO); getsym();
		unique_ptr<Stmt> body(stmt());
		return make_unique<ForStmt>(move(ind), move(from), move(to), move(body), down);
	} CATCH_R(nullptr)
}

static unique_ptr<ReadStmt> read_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(T_READ); getsym();
		check('('); getsym();
		vector<unique_ptr<Expr>> args(expr_list());
		check(')'); getsym();
		return make_unique<ReadStmt>(move(args));
	} CATCH_R(nullptr)
}

static unique_ptr<WriteStmt> write_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		string str;
		unique_ptr<Expr> val;
		check(T_WRITE); getsym();
		check('('); getsym();
		if (tok.sym == STRING) {
			str = move(tok.s);
			getsym();
			if (tok.sym == ',') {
				getsym();
				val = expr();
			}
		} else {
			val = expr();
		}
		check(')'); getsym();
		return make_unique<WriteStmt>(move(str), move(val));
	} CATCH_R(nullptr)
}

static unique_ptr<Expr> lvalue()
{
	X _{BECOMES, ',', ')'};
	try {
		check(IDENT);
		unique_ptr<Expr> e(ident_expr(tok.s));
		getsym();
		if (tok.sym == '[') {
			getsym();
			unique_ptr<Expr> index(expr());
			check(']'); getsym();
			e = make_unique<IndexExpr>(move(e), move(index));
		}
		return e;
	} CATCH_R(nullptr)
}

static vector<unique_ptr<Expr>> expr_list()
{
	X _{')'};
	try {
		vector<unique_ptr<Expr>> ret;
		if (tok.sym != ')') {
			for (;;) {
				ret.emplace_back(expr());
				if (tok.sym != ',')
					break;
				getsym();
			}
		}
		return ret;
	} CATCH_R(vector<unique_ptr<Expr>>())
}

static unique_ptr<Cond> cond()
{
	X _{T_THEN, ';', T_END, T_ELSE, T_WHILE};
	try {
		return disjunction();
	} CATCH_R(nullptr)
}

static unique_ptr<Cond> disjunction()
{
	unique_ptr<Cond> c(conjunction());
	for (;;) {
		if (tok.sym != T_OR)
			return c;
		getsym();
		c = make_unique<CompCond>(CompCond::OR, move(c), conjunction());
	}
	return c;
}

static unique_ptr<Cond> conjunction()
{
	unique_ptr<Cond> c(primary_cond());
	for (;;) {
		if (tok.sym != T_AND)
			return c;
		getsym();
		c = make_unique<CompCond>(CompCond::AND, move(c), primary_cond());
	}
	return c;
}

static unique_ptr<Cond> primary_cond()
{
	if (tok.sym == '(') {
		getsym();
		unique_ptr<Cond> c(cond());
		check(')'); getsym();
		return c;
	}
	if (tok.sym == T_NOT) {
		getsym();
		return make_unique<NegCond>(primary_cond());
	}
	unique_ptr<Expr> e1(expr());
	SimpleCond::Op op;
	switch (tok.sym) {
	case '=':
		op = SimpleCond::EQ;
		break;
	case NE:
		op = SimpleCond::NE;
		break;
	case '<':
		op = SimpleCond::LT;
		break;
	case GE:
		op = SimpleCond::GE;
		break;
	case '>':
		op = SimpleCond::GT;
		break;
	case LE:
		op = SimpleCond::LE;
		break;
	default:
		unexp();
		recover();
	}
	getsym();
	unique_ptr<Expr> e2(expr());
	return make_unique<SimpleCond>(op, move(e1), move(e2));
}

static unique_ptr<Expr> expr()
{
	X _{',', ';', ']', ')', T_END, T_THEN, T_ELSE, '<', GE, '>', LE, '=', NE, T_DO, T_WHILE, T_TO, T_DOWNTO};
	try {
		bool neg = opt_sign();
		unique_ptr<Expr> e(term());
		for (;;) {
			BinaryExpr::Op op;
			switch (tok.sym) {
			case '+':
				op = BinaryExpr::ADD;
				break;
			case '-':
				op = BinaryExpr::SUB;
				break;
			default:
				if (neg)
					return make_unique<UnaryExpr>(UnaryExpr::NEG, move(e));
				return e;
			}
			getsym();
			e = make_unique<BinaryExpr>(op, move(e), term());
		}
	} CATCH_R(nullptr);
}

static unique_ptr<Expr> term()
{
	unique_ptr<Expr> e(factor());
	for (;;) {
		BinaryExpr::Op op;
		switch (tok.sym) {
		case '*':
			op = BinaryExpr::MUL;
			break;
		case '/':
			op = BinaryExpr::DIV;
			break;
		default:
			return e;
		}
		getsym();
		e = make_unique<BinaryExpr>(op, move(e), factor());
	}
}

static unique_ptr<Expr> factor()
{
	unique_ptr<Expr> e;
	Symbol *s;
	switch (tok.sym) {
	case IDENT:
		switch (ntok.sym) {
		case '(':
			s = lookup_checked(tok.s);
			if (s)
				checkprocsym(s);
			getsym();
			getsym();
			if (s) {
				e = make_unique<ApplyExpr>(static_cast<ProcSymbol*>(s), expr_list());
			} else {
				expr_list();
				e = nullptr;
			}
			check(')'); getsym();
			return e;
		case '[':
			e = ident_expr(tok.s);
			getsym();
			getsym();
			e = make_unique<IndexExpr>(move(e), expr());
			check(']'); getsym();
			return e;
		}
		s = lookup_checked(tok.s);
		getsym();
		if (s) {
			if (s->kind == Symbol::PROC)
				return make_unique<ApplyExpr>(static_cast<ProcSymbol*>(s), vector<unique_ptr<Expr>>());
			return make_unique<SymExpr>(s);
		}
		return nullptr;
	case INT:
		e = make_unique<LitExpr>(tok.i);
		getsym();
		return e;
	case '(':
		getsym();
		e = expr();
		check(')'); getsym();
		return e;
	}
	unexp();
	recover();
	return nullptr;
}
