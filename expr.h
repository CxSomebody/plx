struct Expr {
	enum Kind {
		SYM,
		LIT,
		COMP,
	} kind;
	virtual ~Expr();
protected:
	Expr(Kind kind);
};

struct SymExpr: Expr
{
	Symbol *sym;
	SymExpr(Symbol *sym);
};

struct LitExpr: Expr
{
	int lit;
	LitExpr(int lit);
};

struct BinaryExpr: Expr
{
	enum Op {
		ADD,
		SUB,
		MUL,
		DIV,
		INDEX,
	};
	std::unique_ptr<Expr*> left, right;
	BinaryExpr(Expr *left, Expr *right);
};

struct UnaryExpr: Expr
{
	enum Op {
		NEG,
	} op;
	std::unique_ptr<Expr*> sub;
	UnaryExpr(Expr *sub);
};

struct ApplyExpr: Expr
{
	std::unique_ptr<Expr> func;
	std::vector<std::unique_ptr<Expr>> args;
	ApplyExpr(Expr *func, decltype(args) &&args);
};

struct Cond {
	enum Op {
		EQ, NE, LT, GE, GT, LE
	} op;
	Expr *left, *right;
};

struct Stmt {
	enum Kind {
		COMP,
		ASSIGN,
		CALL,
		IF,
		DO_WHILE,
		FOR,
		READ,
		WRITE,
	} kind;
	union {
		struct {
			std::vector<Stmt*> *body;
		} comp;
		struct {
			Expr *var;
			Expr *val;
		} assign;
		struct {
			Symbol *proc;
			std::vector<Expr*> *args;
		} call;
		struct {
			Cond *cond;
			Stmt *st;
			Stmt *sf;
		} if_;
		struct {
			Cond *cond;
			Stmt *body;
		} do_while;
		struct {
			Expr *indvar;
			Expr *from;
			Expr *to;
			Stmt *body;
			bool down;
		} for_;
		struct {
			std::vector<Expr*> *vars;
		} read;
		struct {
			char *str;
			Expr *val;
		} write;
	};
};

typedef std::vector<Expr*> ExprList;

Expr *binary_expr(Expr::Op op, Expr *left, Expr *right);
Expr *unary_expr(Expr::Op op, Expr *right);
Expr *ident_expr(const std::string &name);
Expr *lit_expr(int lit);
Expr *apply_expr(Expr *func, const std::vector<Expr*> &args);
void print_expr(Expr *e);

Stmt *assign_stmt(Expr *var, Expr *val);
Stmt *call_stmt(const std::string &name, const std::vector<Expr*> &args);
Stmt *if_stmt(Cond *cond, Stmt *st);
Stmt *if_stmt(Cond *cond, Stmt *st, Stmt *sf);
Stmt *comp_stmt(const std::vector<Stmt*> &body);
Stmt *do_while_stmt(Cond *cond, Stmt *body);
Stmt *for_stmt(Expr *indvar, Expr *from, Expr *to, Stmt *body, bool down);
Stmt *read_stmt(const std::vector<Expr*> &vars);
Stmt *write_stmt(const std::string &str, Expr *val);
void print_stmt(Stmt *s);

Cond *cond(Cond::Op op, Expr *left, Expr *right);
void print_cond(Cond *c);
