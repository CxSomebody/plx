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

enum class nterm {
	program,
	block,
	const_part,
	const_def,
	constant,
	opt_sign,
	var_part,
	var_decl,
	id_list,
	type,
	basic_type,
	sub_list,
	proc_def,
	proc_header,
	param_list,
	param_group,
	func_def,
	func_header,
	stmt_list,
	stmt,
	call_stmt,
	arg_list,
	expr,
	term,
	factor,
	mul_op,
	add_op,
	lvalue,
	cond,
	rel_op,
	lvalue_list,
	comp_stmt,
	assign_stmt,
	if_stmt,
	do_while_stmt,
	for_stmt,
	read_stmt,
	write_stmt,
};

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

int syntax_errors;

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

static void recover()
{
	for (;;) {
		for (auto it = sync.rbegin();
		     it != sync.rend();
		     it++)
			if (tok.sym == it->first)
				throw it->second;
		getsym();
		fprintf(stderr, "%d:%d: [skipping ‘%s’]\n",
			tok.line, tok.col, tok.spell.c_str());
	}
}

// returns true upon ERROR
static void check(int s)
{
	if (tok.sym != s) {
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
		fprintf(stderr, "%d:%d: syntax error: expected %s, found ‘%s’\n",
			tok.line, tok.col, sname, tok.spell.c_str());
		syntax_errors++;
		recover();
	}
}

#if 0
static void missing(int s)
{
	fprintf(stderr, "%d:%d: missing '%c'\n", tok.line, tok.col, s);
	syntax_errors++;
}
#endif

static void program();
static void block(ProcHeader &&header);
static void const_part();
static void const_def();
static int constant();
static bool opt_sign();
static void var_part();
static void var_decl();
static vector<string> id_list();
static Type *type();
static Type *basic_type();
static void sub_list();
static ProcHeader proc_header();
static ProcHeader func_header();
static vector<Param> param_list();
static vector<Param> param_group();
static unique_ptr<Stmt> stmt();
static unique_ptr<CompStmt> comp_stmt();
static unique_ptr<AssignStmt> assign_stmt();
static unique_ptr<CallStmt> call_stmt();
static unique_ptr<IfStmt> if_stmt();
static unique_ptr<DoWhileStmt> do_while_stmt();
static unique_ptr<ForStmt> for_stmt();
static unique_ptr<ReadStmt> read_stmt();
static unique_ptr<WriteStmt> write_stmt();
static unique_ptr<Expr> lvalue();
static unique_ptr<Expr> factor();
static unique_ptr<Expr> term();
static unique_ptr<Expr> expr();
static vector<unique_ptr<Expr>> expr_list();
static vector<unique_ptr<Expr>> lvalue_list();

void parse()
{
	savetok(lex(), &tok);
	savetok(lex(), &ntok);
	program();
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

static void program()
{
	X _{0};
	try {
		block(ProcHeader("main", vector<Param>{}));
		check('.'); getsym();
		check(0);
	} CATCH
}

static void block(ProcHeader &&header)
{
	X _{';', '.'};
	try {
		//const vector<Param> &params = header.second;
		//push_symtab();
		//def_params(params);
		if (tok.sym == T_CONST) {
			getsym();
			const_part();
		}
		if (tok.sym == T_VAR) {
			getsym();
			var_part();
		}
		sub_list();
		comp_stmt();
		//SymbolTable *symtab = pop_symtab();
	} CATCH
}

static void const_part()
{
	X _{T_VAR, T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		for (;;) {
			const_def();
			if (tok.sym != ',') {
				check(';'); getsym();
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
		printf("const %s = %d\n", name.c_str(), val);
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

static void var_part()
{
	X _{';'};
	try {
		do {
			var_decl();
			check(';'); getsym();
		} while (tok.sym == IDENT);
	} CATCH
}

static void var_decl()
{
	X _{';'};
	try {
		vector<string> names(id_list());
		check(':'); getsym();
		Type *ty = type();
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
		return move(ret);
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

static void sub_list()
{
	X _{T_BEGIN};
	try {
		for (;;) {
			if (tok.sym == T_PROCEDURE) {
				getsym();
				ProcHeader header(proc_header());
				check(';'); getsym();
				block(move(header));
			} else if (tok.sym == T_FUNCTION) {
				getsym();
				ProcHeader header(func_header());
				check(';'); getsym();
				block(move(header));
			} else {
				break;
			}
		}
	} CATCH
}

static ProcHeader proc_header()
{
	X _{T_CONST, T_VAR, T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		check(IDENT);
		string name(move(tok.s));
		getsym();
		ProcHeader header;
		if (tok.sym == '(') {
			getsym();
			vector<Param> params(param_list());
			check(')'); getsym();
		}
		return move(header);
	} CATCH_R(ProcHeader())
}

static ProcHeader func_header()
{
	X _{T_CONST, T_VAR, T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		ProcHeader header(proc_header());
		check(':'); getsym();
		Type *retty = basic_type();
		return move(header);
	} CATCH_R(ProcHeader())
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
		return move(ret);
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
			if (tok.sym != ';')
				break;
			getsym();
		}
		check(T_END); getsym();
		return move(stmts);
	} CATCH_R(nullptr)
}

static unique_ptr<Stmt> stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		switch (tok.sym) {
		case IDENT:
			if (ntok.sym == '(')
				return call_stmt();
			return assign_stmt();
		case T_IF:
			return if_stmt();
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
		check(BECOMES); getsym();
		unique_ptr<Expr> val(expr());
		return make_unique<AssignStmt>(move(var), move(val));
	} CATCH_R(nullptr)
}

static unique_ptr<CallStmt> call_stmt()
{
	return nullptr;
}

static unique_ptr<IfStmt> if_stmt()
{
	return nullptr;
}

static unique_ptr<DoWhileStmt> do_while_stmt()
{
	return nullptr;
}

static unique_ptr<ForStmt> for_stmt()
{
	return nullptr;
}

static unique_ptr<ReadStmt> read_stmt()
{
	return nullptr;
}

static unique_ptr<WriteStmt> write_stmt()
{
	return nullptr;
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
			e = make_unique<BinaryExpr>(BinaryExpr::INDEX, move(e), move(index));
		}
		return move(e);
	} CATCH_R(nullptr)
}
