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
	Type *type = nullptr; // VAR
	int val = 0; // CONST
	Type *rettype = nullptr; // PROC
	Symbol(SymbolKind kind, const std::string &name);
};

struct SymbolTable {
	std::map<std::string, Symbol*> map;
	SymbolTable *up;
	Symbol *lookup(const std::string &name);
};

struct Param {
	std::string name;
	Type *type;
	bool byref;
	Param(const std::string &name, Type *type, bool byref);
};

extern SymbolTable *st;

void def_const(const std::string &name, int val);
void def_vars(const std::vector<std::string> &names, Type *type);
void def_proc(const std::string &name, const std::vector<Param> &names);
void def_func(const std::string &name, const std::vector<Param> &names, Type *rettype);
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
void push_param_group(std::vector<Param> &params,
		      const std::vector<std::string> &names,
		      Type *type,
		      bool byref);
void push_symtab();
void pop_symtab();
