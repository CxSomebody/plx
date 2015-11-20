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
	st->map[name] = s;
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

bool is_proc(const string &name)
{
	Symbol *s = st->lookup(name);
	return s && s->kind == Symbol::PROC && !s->rettype;
}

SymbolTable makeSymbolTable()
{
	return SymbolTable();
}

void push_ntpair_group(std::vector<NameTypePair> &ntpairs,
		       const std::vector<string> &names,
		       Type *type)
{
	for (const string &name: names)
		ntpairs.emplace_back(name, type);
}

void print_symtab(SymbolTable *st)
{
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
		printf("%s %s\n", s->name.c_str(), kindstr);
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
