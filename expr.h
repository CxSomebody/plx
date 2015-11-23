struct Expr {
	enum Kind {
		SYM,
		LIT,
		BINARY,
		UNARY,
		APPLY,
	} kind;
	virtual void print() = 0;
	virtual ~Expr() = 0;
protected:
	Expr(Kind kind);
};

struct SymExpr: Expr
{
	Symbol *sym;
	SymExpr(Symbol *sym);
	void print() override;
};

struct LitExpr: Expr
{
	int lit;
	LitExpr(int lit);
	void print() override;
};

struct BinaryExpr: Expr
{
	enum Op {
		ADD,
		SUB,
		MUL,
		DIV,
		INDEX,
	} op;
	std::unique_ptr<Expr> left, right;
	BinaryExpr(Expr *left, Expr *right);
	void print() override;
};

struct UnaryExpr: Expr
{
	enum Op {
		NEG,
	} op;
	std::unique_ptr<Expr> sub;
	UnaryExpr(Expr *sub);
	void print() override;
};

struct ApplyExpr: Expr
{
	std::unique_ptr<Expr> func;
	std::vector<std::unique_ptr<Expr>> args;
	ApplyExpr(Expr *func, decltype(args) &&args);
	void print() override;
};

struct Cond {
	enum Op {
		EQ, NE, LT, GE, GT, LE
	} op;
	std::unique_ptr<Expr> left, right;
	Cond(Op op, Expr *left, Expr *right);
	void print();
};

struct Stmt {
	enum Kind {
		EMPTY,
		COMP,
		ASSIGN,
		CALL,
		IF,
		DO_WHILE,
		FOR,
		READ,
		WRITE,
	} kind;
	virtual ~Stmt() = 0;
	virtual void print() = 0;
protected:
	Stmt(Kind kind);
};

struct EmptyStmt: Stmt
{
	EmptyStmt();
	void print() override;
};

struct CompStmt: Stmt
{
	std::vector<std::unique_ptr<Stmt>> body;
	CompStmt(decltype(body) &&body);
	void print() override;
};

struct AssignStmt: Stmt
{
	std::unique_ptr<Expr> var, val;
	AssignStmt(Expr *var, Expr *val);
	void print() override;
};

struct CallStmt: Stmt
{
	Symbol *proc;
	std::vector<std::unique_ptr<Expr>> args;
	CallStmt(decltype(args) &&args);
	void print() override;
};

struct IfStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> st, sf;
	IfStmt(Cond *cond, Stmt *st, Stmt *sf);
	void print() override;
};

struct DoWhileStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> body;
	DoWhileStmt(Cond *cond, Stmt *body);
	void print() override;
};

struct ForStmt: Stmt
{
	std::unique_ptr<Expr> indvar, from, to;
	std::unique_ptr<Stmt> body;
	bool down;
	ForStmt(Expr *indvar, Expr *from, Expr *to, Stmt *body, bool down);
	void print() override;
};

struct ReadStmt: Stmt
{
	std::vector<std::unique_ptr<Expr>> vars;
	ReadStmt(decltype(vars) &&vars);
	void print() override;
};

struct WriteStmt: Stmt
{
	std::string str;
	std::unique_ptr<Expr> val;
	WriteStmt(const std::string &str, Expr *val);
	void print() override;
};

Expr *ident_expr(const std::string &name);
