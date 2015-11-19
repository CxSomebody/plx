struct Type {
};

struct Symbol {
};

struct SymbolTable {
	SymbolTable *up;
};

void def_const(SymbolTable &st, const std::string &name, int val);
void def_vars(SymbolTable &st, const std::vector<std::string> &names, Type *type);
Type *int_type();
Type *char_type();
Type *array_type(Type *elty, int n);
bool is_proc(const std::string &ident);
SymbolTable makeSymbolTable();
