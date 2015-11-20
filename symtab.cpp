#include <cassert>
#include <map>
#include <string>
#include <vector>
#include "symtab.h"

using namespace std;

Symbol::Symbol(SymbolKind kind, const string &name):
	kind(kind), name(name)
{
}

Symbol *SymbolTable::lookup(const string &name)
{
	if (map.count(name))
		return map[name];
	if (up)
		return up->lookup(name);
	return nullptr;
}

NameTypePair::NameTypePair(const std::string &name, Type *type):
	name(name), type(type)
{
}

SymbolTable *st;

void def_const(const string &name, int val)
{
	Symbol *s = new Symbol(Symbol::CONST, name);
	s->val = val;
	st->map[name] = s;
}

void def_vars(const vector<string> &names, Type *type)
{
	for (const string &name: names) {
		Symbol *s = new Symbol(Symbol::VAR, name);
		s->type = type;
		st->map[name] = s;
	}
}

void def_proc(const string &name, const vector<NameTypePair> &names)
{
	Symbol *s = new Symbol(Symbol::PROC, name);
	st->map[name] = s;
}

void def_func(const string &name, const vector<NameTypePair> &names, Type *rettype)
{
	Symbol *s = new Symbol(Symbol::PROC, name);
	s->rettype = rettype;
	st->map[name] = s;
}

Type *int_type()
{
	static Type type { Type::INT };
	return &type;
}

Type *char_type()
{
	static Type type { Type::CHAR };
	return &type;
}

Type *array_type(Type *elty, int n)
{
	// TODO memory leak
	return new Type { Type::ARRAY, elty };
}

bool is_proc(const string &name)
{
	Symbol *s = st->lookup(name);
	return s && s->kind == Symbol::PROC && !s->rettype;
}

void push_ntpair_group(std::vector<NameTypePair> &ntpairs,
		       const std::vector<string> &names,
		       Type *type)
{
	for (const string &name: names)
		ntpairs.emplace_back(name, type);
}

void print_type(Type *ty)
{
	assert(ty);
	const char *kindstr;
	switch (ty->kind) {
	case Type::INT:
		printf("int");
		break;
	case Type::CHAR:
		printf("char");
		break;
	case Type::ARRAY:
		printf("array of ");
		print_type(ty->elemtype);
		break;
	default:
		assert(0);
	}
}

void print_symtab(SymbolTable *st)
{
	assert(st);
	for (auto &pair: st->map) {
		Symbol *s = pair.second;
		const char *kindstr;
		switch (s->kind) {
		case Symbol::VAR:
			kindstr = "VAR";
			break;
		case Symbol::CONST:
			kindstr = "CONST";
			break;
		case Symbol::PROC:
			kindstr = "PROC";
			break;
		default:
			assert(0);
		}
		printf("%s %s", s->name.c_str(), kindstr);
		if (s->kind == Symbol::VAR) {
			putchar(' ');
			print_type(s->type);
		} else if (s->kind == Symbol::CONST) {
			printf(" %d", s->val);
		}
		putchar('\n');
	}
}

void push_symtab()
{
	SymbolTable *newst = new SymbolTable();
	newst->up = st;
	st = newst;
}

void pop_symtab()
{
	print_symtab(st);
	SymbolTable *savedst = st;
	st = st->up;
	delete savedst;
}
