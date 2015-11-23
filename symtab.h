struct Type {
	enum TypeKind {
		INT,
		CHAR,
		ARRAY,
	} kind;
	Type *elemtype; // ARRAY
};

struct Symbol {
	enum SymbolKind {
		VAR,
		CONST,
		PROC,
	} kind;
	std::string name;
	Type *type; // VAR
	int val; // CONST
	Type *rettype; // PROC
	Symbol(SymbolKind kind, const std::string &name);
	void print();
};

struct Stmt;

Symbol *var_symbol(const std::string &name, Type *type);
Symbol *const_symbol(const std::string &name, int val);
Symbol *proc_symbol(const std::string &name, Type *rettype);

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
	Block(const std::string &name, const std::vector<std::unique_ptr<Block>> &subs, const std::vector<std::unique_ptr<Stmt>> &stmts, SymbolTable *symtab);
};

struct Param {
	std::string name;
	Type *type;
	bool byref;
	Param(const std::string &name, Type *type, bool byref);
};

typedef std::vector<Param> ParamList;

extern SymbolTable *symtab;

void def_const(const std::string &name, int val);
void def_vars(const std::vector<std::string> &names, Type *type);
void def_proc(const std::string &name, const std::vector<Param> &names);
void def_func(const std::string &name, const std::vector<Param> &names, Type *rettype);
void def_params(const ParamList &params);
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
void push_param_group(std::vector<Param> &params,
		      const std::vector<std::string> &names,
		      Type *type,
		      bool byref);
void push_symtab();
SymbolTable *pop_symtab();
void translate(Block *blk);
const char *entry_name();
