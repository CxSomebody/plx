struct Type {
	enum Kind {
		INT,
		CHAR,
		ARRAY,
	} kind;
	virtual ~Type();
	virtual void print() = 0;
	virtual int size() = 0;
	virtual int align() = 0;
protected:
	Type(Kind kind);
};

struct IntType: Type
{
	IntType();
	void print() override;
	int size() override;
	int align() override;
};

struct CharType: Type
{
	CharType();
	void print() override;
	int size() override;
	int align() override;
};

struct ArrayType: Type
{
	Type *elemtype;
	int nelem;
	ArrayType(Type *elemtype, int nelem);
	void print() override;
	int size() override;
	int align() override;
};

struct Symbol {
	enum Kind {
		VAR,
		CONST,
		PROC,
	} kind;
	std::string name;
	void print();
protected:
	Symbol(Kind kind, const std::string &name);
};

struct VarSymbol: Symbol
{
	Type *type;
	VarSymbol(const std::string &name, Type *type);
};

struct ConstSymbol: Symbol
{
	int val;
	ConstSymbol(const std::string &name, int val);
};

struct ProcSymbol: Symbol
{
	Type *rettype;
	ProcSymbol(const std::string &name, Type *rettype);
};

struct Stmt;

struct SymbolTable {
	std::map<std::string, Symbol*> map;
	SymbolTable *up;
	Symbol *lookup(const std::string &name);
};

struct Block {
	std::string name;
	std::vector<std::unique_ptr<Block>> subs;
	std::vector<std::unique_ptr<Stmt>> stmts;
	SymbolTable *symtab;
	Block(std::string &&name,
	      std::vector<std::unique_ptr<Block>> &&subs,
	      std::vector<std::unique_ptr<Stmt>> &&stmts,
	      SymbolTable *symtab);
	void print(int level);
};

struct Param {
	std::string name;
	Type *type;
	bool byref;
	Param(const std::string &name, Type *type, bool byref);
};

typedef std::pair<std::string, std::vector<Param>> ProcHeader;

extern SymbolTable *symtab;

void def_const(const std::string &name, int val);
void def_vars(const std::vector<std::string> &names, Type *type);
void def_func(const ProcHeader &header, Type *rettype);
void def_params(const std::vector<Param> &params);
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
std::vector<Param> param_group(std::vector<std::string> &&names, Type *type, bool byref);
void push_symtab();
SymbolTable *pop_symtab();
void translate(std::unique_ptr<Block> &&blk);
Symbol *lookup(const std::string &name);

struct Expr {
	enum Kind {
		SYM,
		LIT,
		BINARY,
		UNARY,
		APPLY,
	} kind;
	virtual void print() = 0;
	virtual ~Expr();
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
	BinaryExpr(Op op, std::unique_ptr<Expr> &&left, std::unique_ptr<Expr> &&right);
	void print() override;
};

struct UnaryExpr: Expr
{
	enum Op {
		NEG,
	} op;
	std::unique_ptr<Expr> sub;
	UnaryExpr(Op op, std::unique_ptr<Expr> &&sub);
	void print() override;
};

struct ApplyExpr: Expr
{
	std::unique_ptr<Expr> func;
	std::vector<std::unique_ptr<Expr>> args;
	ApplyExpr(std::unique_ptr<Expr> &&func, decltype(args) &&args);
	void print() override;
};

struct Cond {
	enum Op {
		EQ, NE, LT, GE, GT, LE
	} op;
	std::unique_ptr<Expr> left, right;
	Cond(Op op, std::unique_ptr<Expr> &&left, std::unique_ptr<Expr> &&right);
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
	virtual ~Stmt();
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
	AssignStmt(std::unique_ptr<Expr> &&var, std::unique_ptr<Expr> &&val);
	void print() override;
};

struct CallStmt: Stmt
{
	Symbol *proc;
	std::vector<std::unique_ptr<Expr>> args;
	CallStmt(Symbol *proc, decltype(args) &&args);
	void print() override;
};

struct IfStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> st, sf;
	IfStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&st);
	IfStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&st, std::unique_ptr<Stmt> &&sf);
	void print() override;
};

struct DoWhileStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> body;
	DoWhileStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&body);
	void print() override;
};

struct ForStmt: Stmt
{
	std::unique_ptr<Expr> indvar, from, to;
	std::unique_ptr<Stmt> body;
	bool down;
	ForStmt(std::unique_ptr<Expr> &&indvar, std::unique_ptr<Expr> &&from, std::unique_ptr<Expr> &&to, std::unique_ptr<Stmt> &&body, bool down);
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
	WriteStmt(std::string &&str, std::unique_ptr<Expr> &&val);
	void print() override;
};

std::unique_ptr<Expr> ident_expr(const std::string &name);
