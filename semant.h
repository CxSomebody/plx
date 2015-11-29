struct Type {
	enum Kind {
		//ERROR,
		INT,
		CHAR,
		ARRAY,
	} kind;
	virtual ~Type();
	virtual void print() const = 0;
	virtual int size() const = 0;
	virtual int align() const = 0;
protected:
	Type(Kind kind);
};

Type *char_type();
Type *int_type();
Type *binexprtype(Type *a, Type *b);

#if 0
struct ErrorType: Type
{
	ErrorType(): Type(ERROR) {}
	void print() const override
	{
		printf("<error-type>");
	}
	int size() const override
	{
		return 0;
	}
	int align() const override
	{
		return 0;
	}
};
#endif

struct IntType: Type
{
	IntType();
	void print() const override;
	int size() const override;
	int align() const override;
};

struct CharType: Type
{
	CharType();
	void print() const override;
	int size() const override;
	int align() const override;
};

struct ArrayType: Type
{
	Type *elemtype;
	int nelem;
	ArrayType(Type *elemtype, int nelem);
	void print() const override;
	int size() const override;
	int align() const override;
};

struct Symbol {
	enum Kind {
		VAR,
		CONST,
		PROC,
	} kind;
	std::string name;
	Type *type;
	void print() const;
protected:
	Symbol(Kind kind, const std::string &name, Type *type);
};

struct VarSymbol: Symbol
{
	Type *type;
	int level;
	int offset; // relative to bp
	VarSymbol(const std::string &name, Type *type, int level, int offset):
		Symbol(VAR, name, type), type(type), level(level), offset(offset) {}
};

struct ConstSymbol: Symbol
{
	int val;
	ConstSymbol(const std::string &name, int val):
		Symbol(CONST, name, int_type()), val(val) {}
};

struct ProcSymbol: Symbol
{
	Type *rettype;
	ProcSymbol(const std::string &name, Type *rettype):
		Symbol(PROC, name, nullptr), rettype(rettype) {}
};

struct Stmt;

struct SymbolTable {
	std::map<std::string, Symbol*> map;
	SymbolTable *up;
	int level;
	SymbolTable(SymbolTable *up, int level): up(up), level(level) {}
	Symbol *lookup(const std::string &name);
	void print(int level) const;
};

struct Block {
	std::string name;
	std::vector<std::unique_ptr<Block>> subs;
	std::vector<VarSymbol*> vars;
	std::vector<std::unique_ptr<Stmt>> stmts;
	SymbolTable *symtab;
	Block(std::string &&name,
	      decltype(subs) &&subs,
	      decltype(vars) &&vars,
	      decltype(stmts)&&stmts,
	      SymbolTable *symtab):
		name(move(name)),
		subs(move(subs)),
		vars(move(vars)),
		stmts(move(stmts)),
		symtab(symtab) {}
	void print(int level) const;
	void translate();
	void allocaddr();
};

struct Param {
	std::string name;
	Type *type;
	bool byref;
	Param(const std::string &name, Type *type, bool byref);
};

struct ProcHeader {
	std::string name;
	std::vector<Param> params;
	Type *rettype;
	ProcHeader() = default;
	ProcHeader(const std::string &name, std::vector<Param> &&params, Type *rettype):
		name(name), params(std::move(params)), rettype(rettype) {}
};

void def_const(const std::string &name, int val);
VarSymbol *def_var(const std::string &name, Type *type);
void def_func(const ProcHeader &header, Type *rettype);
void def_params(const std::vector<Param> &params, int level);
Type *error_type();
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
std::vector<Param> param_group(std::vector<std::string> &&names, Type *type, bool byref);
void push_symtab();
SymbolTable *pop_symtab();
void translate_all(std::unique_ptr<Block> &&blk);
Symbol *lookup(const std::string &name);

struct Operand;
struct LabelOperand;
class TranslateEnv;

struct Expr {
	enum Kind {
		SYM,
		LIT,
		BINARY,
		UNARY,
		APPLY,
	} kind;
	Type *type;
	virtual void print() const = 0;
	virtual Operand *translate(TranslateEnv &env) const = 0;
	virtual ~Expr();
protected:
	Expr(Kind kind, Type *type);
};

struct SymExpr: Expr
{
	Symbol *sym;
	SymExpr(Symbol *sym);
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct LitExpr: Expr
{
	int lit;
	LitExpr(int lit);
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
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
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct UnaryExpr: Expr
{
	enum Op {
		NEG,
	} op;
	std::unique_ptr<Expr> sub;
	UnaryExpr(Op op, std::unique_ptr<Expr> &&sub);
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct ApplyExpr: Expr
{
	ProcSymbol *func;
	std::vector<std::unique_ptr<Expr>> args;
	ApplyExpr(ProcSymbol *func, decltype(args) &&args);
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct Cond {
	enum Op {
		EQ, NE, LT, GE, GT, LE
	} op;
	std::unique_ptr<Expr> left, right;
	Cond(Op op, std::unique_ptr<Expr> &&left, std::unique_ptr<Expr> &&right);
	void print() const;
	void translate(TranslateEnv &env, LabelOperand *label, bool negate) const;
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
	virtual void print() const = 0;
	virtual void translate(TranslateEnv &env) const = 0;
protected:
	Stmt(Kind kind);
};

struct EmptyStmt: Stmt
{
	EmptyStmt();
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct CompStmt: Stmt
{
	std::vector<std::unique_ptr<Stmt>> body;
	CompStmt(decltype(body) &&body);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct AssignStmt: Stmt
{
	std::unique_ptr<Expr> var, val;
	AssignStmt(std::unique_ptr<Expr> &&var, std::unique_ptr<Expr> &&val);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct CallStmt: Stmt
{
	ProcSymbol *proc;
	std::vector<std::unique_ptr<Expr>> args;
	CallStmt(ProcSymbol *proc, decltype(args) &&args);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct IfStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> st, sf;
	IfStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&st);
	IfStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&st, std::unique_ptr<Stmt> &&sf);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct WhileStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> body;
	WhileStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&body);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct DoWhileStmt: Stmt
{
	std::unique_ptr<Cond> cond;
	std::unique_ptr<Stmt> body;
	DoWhileStmt(std::unique_ptr<Cond> &&cond, std::unique_ptr<Stmt> &&body);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct ForStmt: Stmt
{
	std::unique_ptr<Expr> indvar, from, to;
	std::unique_ptr<Stmt> body;
	bool down;
	ForStmt(std::unique_ptr<Expr> &&indvar, std::unique_ptr<Expr> &&from, std::unique_ptr<Expr> &&to, std::unique_ptr<Stmt> &&body, bool down);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct ReadStmt: Stmt
{
	std::vector<std::unique_ptr<Expr>> vars;
	ReadStmt(decltype(vars) &&vars);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};

struct WriteStmt: Stmt
{
	std::string str;
	std::unique_ptr<Expr> val;
	WriteStmt(std::string &&str, std::unique_ptr<Expr> &&val);
	void print() const override;
	void translate(TranslateEnv &env) const override;
};
