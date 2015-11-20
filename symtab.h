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

struct NameTypePair {
	std::string name;
	Type *type;
	NameTypePair(const std::string &name, Type *type);
};

extern SymbolTable *st;

void def_const(const std::string &name, int val);
void def_vars(const std::vector<std::string> &names, Type *type);
void def_proc(const std::string &name, const std::vector<NameTypePair> &names);
void def_func(const std::string &name, const std::vector<NameTypePair> &names, Type *rettype);
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
void push_ntpair_group(std::vector<NameTypePair> &ntpairs,
		       const std::vector<std::string> &names,
		       Type *type);
void push_symtab();
void pop_symtab();
