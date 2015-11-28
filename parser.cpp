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

static void error(int s)
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
	fprintf(stderr, "%d:%d: expected %s, found ‘%s’\n",
		tok.line, tok.col, sname, tok.spell.c_str());
	syntax_errors++;
}

// returns true upon ERROR
static void check(int s)
{
	if (tok.sym != s) {
		error(s);
		recover();
	}
}

static void missing(int s)
{
	fprintf(stderr, "%d:%d: missing '%c' before ‘%s’\n",
		tok.line, tok.col, s, tok.spell.c_str());
	syntax_errors++;
}

static void skip()
{
	fprintf(stderr, "%d:%d: ignoring extra token ‘%s’\n",
		tok.line, tok.col, tok.spell.c_str());
	syntax_errors++;
	getsym();
}

static unique_ptr<Block> program();
static unique_ptr<Block> block(ProcHeader &&header);
static void const_part();
static void const_def();
static int constant();
static bool opt_sign();
static vector<pair<string, Type*>> var_part();
static void var_decl(vector<pair<string, Type*>> &list);
static vector<string> id_list();
static Type *type();
static Type *basic_type();
static vector<unique_ptr<Block>> sub_list();
static ProcHeader proc_header(bool isfunc);
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
static unique_ptr<Cond> cond();

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
		unique_ptr<Block> blk(block(ProcHeader("main", vector<Param>{})));
		check('.'); getsym();
		check(0);
		return blk;
	} CATCH_R(nullptr)
}

static unique_ptr<Block> block(ProcHeader &&header)
{
	X _{';', '.'};
	try {
		push_symtab();
		def_params(header.second);
		if (tok.sym == T_CONST) {
			getsym();
			const_part();
		}
		vector<pair<string, Type*>> vars;
		if (tok.sym == T_VAR) {
			getsym();
			vars = var_part();
		}
		vector<unique_ptr<Block>> subs(sub_list());
		unique_ptr<CompStmt> body(comp_stmt());
		SymbolTable *symtab = pop_symtab();
		return make_unique<Block>
			(move(header.first),
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
					error(';');
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

static vector<pair<string, Type*>> var_part()
{
	X _{T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		vector<pair<string, Type*>> ret;
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
				error(';');
			}
		} while (tok.sym == IDENT);
		return ret;
	} CATCH_R((vector<pair<string, Type*>>()))
}

static void var_decl(vector<pair<string, Type*>> &list)
{
	X _{';'};
	try {
		vector<string> names(id_list());
		check(':'); getsym();
		Type *ty = type();
		for (const string &name: names) {
			def_var(name, ty);
			list.emplace_back(name, ty);
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
				ProcHeader header(proc_header(isfunc));
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
					error(';');
				}
				ret.emplace_back(block(move(header)));
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
					error(';');
				}
			} else {
				return ret;
			}
		}
	} CATCH_R(vector<unique_ptr<Block>>())
}

static ProcHeader proc_header(bool isfunc)
{
	X _{T_CONST, T_VAR, T_PROCEDURE, T_FUNCTION, T_BEGIN};
	try {
		ProcHeader header;
		check(IDENT);
		header.first = move(tok.s);
		getsym();
		if (tok.sym == '(') {
			getsym();
			header.second = param_list();
			check(')'); getsym();
		}
		if (isfunc) {
			check(':'); getsym();
			Type *retty = basic_type();
			def_func(header, retty);
		} else {
			def_func(header, nullptr);
		}
		return header;
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
				error(';');
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
			    ntok.sym == '[')
				return assign_stmt();
			return call_stmt();
		case T_BEGIN:
			return comp_stmt();
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
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(IDENT);
		Symbol *proc = lookup(tok.s);
		getsym();
		vector<unique_ptr<Expr>> args;
		if (tok.sym == '(') {
			check('('); getsym();
			args = expr_list();
			check(')'); getsym();
		}
		return make_unique<CallStmt>(proc, move(args));
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
			skip();
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

static unique_ptr<DoWhileStmt> do_while_stmt()
{
	X _{';', T_END, T_ELSE, T_WHILE};
	try {
		check(T_DO); getsym();
		unique_ptr<Stmt> body(stmt());
		if (tok.sym == T_WHILE) {
			getsym();
		} else if (tok.sym == ';' && ntok.sym == T_WHILE) {
			skip();
			getsym();
		} else {
			error(T_WHILE);
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
			// TODO report error
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
			e = make_unique<BinaryExpr>(BinaryExpr::INDEX, move(e), move(index));
		}
		return e;
	} CATCH_R(nullptr)
}

static vector<unique_ptr<Expr>> expr_list()
{
	X _{')'};
	try {
		vector<unique_ptr<Expr>> ret;
		for (;;) {
			ret.emplace_back(expr());
			if (tok.sym != ',')
				break;
			getsym();
		}
		return ret;
	} CATCH_R(vector<unique_ptr<Expr>>())
}

static unique_ptr<Cond> cond()
{
	X _{T_THEN, ';', T_END, T_ELSE, T_WHILE};
	try {
		unique_ptr<Expr> e1(expr());
		Cond::Op op;
		switch (tok.sym) {
		case '=':
			op = Cond::EQ;
			break;
		case NE:
			op = Cond::NE;
			break;
		case '<':
			op = Cond::LT;
			break;
		case GE:
			op = Cond::GE;
			break;
		case '>':
			op = Cond::GT;
			break;
		case LE:
			op = Cond::LE;
			break;
		default:
			// TODO report error
			recover();
		}
		getsym();
		unique_ptr<Expr> e2(expr());
		return make_unique<Cond>(op, move(e1), move(e2));
	} CATCH_R(nullptr)
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
	switch (tok.sym) {
	case IDENT:
		e = ident_expr(tok.s);
		getsym();
		switch (tok.sym) {
		case '(':
			getsym();
			e = make_unique<ApplyExpr>(move(e), expr_list());
			check(')'); getsym();
			return e;
		case '[':
			getsym();
			e = make_unique<BinaryExpr>(BinaryExpr::INDEX, move(e), expr());
			check(']'); getsym();
			return e;
		}
		return e;
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
	// TODO report error
	recover();
	return nullptr;
}
