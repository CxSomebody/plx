#include <string>
#include <vector>
#include "symtab.h"

using namespace std;

void def_const(SymbolTable &st, const string &name, int val)
{
}

void def_vars(SymbolTable &st, const std::vector<std::string> &names, Type *type)
{
}

Type *int_type()
{
	return nullptr;
}

Type *char_type()
{
	return nullptr;
}

Type *array_type(Type *elty, int n)
{
	return nullptr;
}

bool is_proc(const string &ident)
{
	return false;
}

SymbolTable makeSymbolTable()
{
	return SymbolTable();
}
