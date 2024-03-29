struct Type {
	enum Kind {
		//ERROR,
		INT,
		CHAR,
		ARRAY,
	} kind;
	virtual ~Type();
	virtual std::string tostr() const = 0;
	virtual int size() const = 0;
	virtual int align() const = 0;
	bool is_scalar() const;
protected:
	Type(Kind kind);
};

Type *char_type();
Type *int_type();
Type *binexprtype(Type *a, Type *b);
Type *elemtype(Type *ty);

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
	std::string tostr() const override;
	int size() const override;
	int align() const override;
};

struct CharType: Type
{
	CharType();
	std::string tostr() const override;
	int size() const override;
	int align() const override;
};

struct ArrayType: Type
{
	Type *elemtype;
	int nelem;
	ArrayType(Type *elemtype, int nelem);
	std::string tostr() const override;
	int size() const override;
	int align() const override;
};

struct Param {
	std::string name;
	Type *type;
	bool byref;
	Param(const std::string &name, Type *type, bool byref);
};

struct Symbol {
	enum Kind {
		VAR,
		CONST,
		PROC,
	} kind;
	std::string name;
	Type *type;
	int level;
	void print() const;
protected:
	Symbol(Kind kind, const std::string &name, Type *type, int level):
		kind(kind), name(name), type(type), level(level) {}
};

struct VarSymbol: Symbol
{
	Type *type;
	int offset = 0; // relative to bp
	int scalar_id = -1;
	bool isref;
	VarSymbol(const std::string &name, Type *type, int level, bool isref):
		Symbol(VAR, name, type, level), type(type), isref(isref) {}
};

struct ConstSymbol: Symbol
{
	int val;
	ConstSymbol(const std::string &name, int level, int val):
		Symbol(CONST, name, int_type(), level), val(val) {}
};

struct ProcSymbol;

struct SymbolTable {
	std::map<std::string, Symbol*> map;
	SymbolTable *up;
	ProcSymbol *proc;
	int level;
	SymbolTable(SymbolTable *up, ProcSymbol *proc, int level): up(up), proc(proc), level(level) {}
	Symbol *lookup(const std::string &name);
	void print(int level) const;
};

struct ProcSymbol: Symbol
{
	std::vector<Param> params;
	Type *rettype;
	std::string decorated_name;
	ProcSymbol(const std::string &name, ProcSymbol *up, const std::vector<Param> &params, Type *rettype):
		Symbol(PROC, name, nullptr, up ? up->level+1 : 1),
		params(params),
		rettype(rettype),
		decorated_name(up ? up->decorated_name+'$'+name : name) {}
};

struct Stmt;

struct Block {
	ProcSymbol *proc;
	std::vector<std::unique_ptr<Block>> subs;
	std::vector<VarSymbol*> params;
	std::vector<VarSymbol*> vars;
	std::vector<std::unique_ptr<Stmt>> stmts;
	SymbolTable *symtab;
	Block(ProcSymbol *proc,
	      decltype(subs) &&subs,
	      decltype(params) &&params,
	      decltype(vars) &&vars,
	      decltype(stmts)&&stmts,
	      SymbolTable *symtab):
		proc(proc),
		subs(move(subs)),
		params(move(params)),
		vars(move(vars)),
		stmts(move(stmts)),
		symtab(symtab) {}
	void print(int level) const;
};

void def_const(const std::string &name, int val);
VarSymbol *def_var(const std::string &name, Type *type);
ProcSymbol *def_func(const std::string &name, const std::vector<Param> &params, Type *rettype);
std::vector<VarSymbol*> def_params(const std::vector<Param> &params);
Type *error_type();
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
std::vector<Param> param_group(std::vector<std::string> &&names, Type *type, bool byref);
void push_symtab(ProcSymbol *proc);
SymbolTable *pop_symtab();
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
		INDEX,
	} kind;
	Type *type;
	virtual void print() const = 0;
	virtual Operand *translate(TranslateEnv &env) const = 0;
	virtual ~Expr() {}
	bool is_lvalue() const;
protected:
	Expr(Kind kind, Type *type):
		kind(kind), type(type) {}
};

struct SymExpr: Expr
{
	Symbol *sym;
	SymExpr(Symbol *sym):
		Expr(SYM, sym ? sym->type : nullptr), sym(sym) {}
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct LitExpr: Expr
{
	int lit;
	LitExpr(int lit):
		Expr(LIT, int_type()), lit(lit) {}
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
	} op;
	std::unique_ptr<Expr> left, right;
	BinaryExpr(Op op, std::unique_ptr<Expr> &&left, std::unique_ptr<Expr> &&right):
		Expr(BINARY, left && right ? binexprtype(left->type, right->type) : nullptr),
		op(op), left(move(left)), right(move(right)) {}
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct IndexExpr: Expr
{
	std::unique_ptr<Expr> array, index;
	IndexExpr(std::unique_ptr<Expr> array, std::unique_ptr<Expr> index):
		Expr(INDEX, array ? elemtype(array->type) : nullptr),
		array(std::move(array)), index(std::move(index)) {}
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct UnaryExpr: Expr
{
	enum Op {
		NEG,
	} op;
	std::unique_ptr<Expr> sub;
	UnaryExpr(Op op, std::unique_ptr<Expr> &&sub):
		Expr(UNARY, sub ? sub->type : nullptr), op(op), sub(move(sub)) {}
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct ApplyExpr: Expr
{
	ProcSymbol *func;
	std::vector<std::unique_ptr<Expr>> args;
	ApplyExpr(ProcSymbol *func, decltype(args) &&args):
		Expr(APPLY, func ? func->rettype : nullptr), func(func), args(move(args)) {}
	void print() const override;
	Operand *translate(TranslateEnv &env) const override;
};

struct Cond {
	enum Kind {
		SIMPLE,
		COMP,
		NEG,
	} kind;
	virtual void print() const = 0;
	virtual void translate(TranslateEnv &env, LabelOperand *label, bool negate) const = 0;
	Cond(Kind kind): kind(kind) {}
};

struct SimpleCond: Cond
{
	enum Op {
		EQ, NE, LT, GE, GT, LE
	} op;
	std::unique_ptr<Expr> left, right;
	void print() const override;
	void translate(TranslateEnv &env, LabelOperand *label, bool negate) const override;
	SimpleCond(Op op, std::unique_ptr<Expr> &&left, std::unique_ptr<Expr> &&right):
		Cond(SIMPLE), op(op), left(std::move(left)), right(std::move(right)) {}
};

struct CompCond: Cond
{
	enum Op {
		AND,
		OR,
	} op;
	std::unique_ptr<Cond> left, right;
	void print() const override;
	void translate(TranslateEnv &env, LabelOperand *label, bool negate) const override;
	CompCond(Op op, std::unique_ptr<Cond> &&left, std::unique_ptr<Cond> &&right):
		Cond(COMP), op(op), left(std::move(left)), right(std::move(right)) {}
};

struct NegCond: Cond
{
	std::unique_ptr<Cond> sub;
	void print() const override;
	void translate(TranslateEnv &env, LabelOperand *label, bool negate) const override;
	NegCond(std::unique_ptr<Cond> &&sub): Cond(NEG), sub(std::move(sub)) {}
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
