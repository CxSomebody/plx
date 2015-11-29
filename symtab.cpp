#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

Symbol::Symbol(Kind kind, const string &name, Type *type): kind(kind), name(name), type(type) {}

void Symbol::print() const
{
	if (this) printf("%s", name.c_str());
	else printf("<error>");
}

Symbol *SymbolTable::lookup(const string &name)
{
	if (map.count(name))
		return map[name];
	if (up)
		return up->lookup(name);
	return nullptr;
}

Param::Param(const string &name, Type *type, bool byref):
	name(name), type(type), byref(byref)
{
}

SymbolTable *symtab;

Symbol *lookup(const string &name)
{
	return symtab->lookup(name);
}

bool check_redef(const string &name)
{
	if (symtab->map.count(name)) {
		fprintf(stderr, "error: redefinition of ‘%s’\n", name.c_str());
		return false;
	}
	return true;
}

void def_const(const string &name, int val)
{
	if (check_redef(name)) {
		symtab->map[name] = new ConstSymbol(name, val);
	}
}

VarSymbol *def_var(const string &name, Type *type)
{
	if (check_redef(name)) {
		VarSymbol *vs = new VarSymbol(name, type, symtab->level, 0);
		symtab->map[name] = vs;
		return vs;
	}
	return nullptr;
}

void def_func(const ProcHeader &header, Type *rettype)
{
	const string &name = header.name;
	if (check_redef(name)) {
		symtab->map[name] = new ProcSymbol(name, rettype);
	}
}

void def_params(const vector<Param> &params, int level)
{
	int offset = 8+level*4;
	for (const Param &p: params) {
		const string &name = p.name;
		if (check_redef(name)) {
			symtab->map[name] = new VarSymbol(name, p.type, symtab->level, offset);
			offset += 4;
		}
	}
}

#if 0
Type *error_type()
{
	static ErrorType type;
	return &type;
}
#endif

Type *int_type()
{
	static IntType type;
	return &type;
}

Type *char_type()
{
	static CharType type;
	return &type;
}

Type *array_type(Type *elty, int n)
{
	// TODO memory leak
	return new ArrayType(elty, n);
}

vector<Param> param_group(vector<string> &&names, Type *type, bool byref)
{
	vector<Param> params;
	for (string &name: names)
		params.emplace_back(move(name), type, byref);
	return params;
}

static void indent(int level)
{
	for (int i=0; i<level; i++)
		printf("  ");
};

void SymbolTable::print(int level) const
{
	for (auto &pair: map) {
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
		indent(level);
		printf("%s %s", s->name.c_str(), kindstr);
		if (s->kind == Symbol::VAR) {
			putchar(' ');
			VarSymbol *vs = static_cast<VarSymbol*>(s);
			vs->type->print();
			printf(" level=%d offset=%+d", vs->level, vs->offset);
		} else if (s->kind == Symbol::CONST) {
			printf(" %d", static_cast<ConstSymbol*>(s)->val);
		}
		putchar('\n');
	}
}

void push_symtab()
{
	int nextlevel = symtab ? symtab->level+1 : 0;
	SymbolTable *newst = new SymbolTable(symtab, nextlevel);
	symtab = newst;
}

SymbolTable *pop_symtab()
{
	SymbolTable *savedst = symtab;
	symtab = symtab->up;
	return savedst;
}

void print_stmt(Stmt *s);

void Block::print(int level) const
{
	indent(level);
	printf("%s\n", name.c_str());
	symtab->print(level);
	indent(level);
	printf("{\n");
	level++;
	for (auto &sub: subs) {
		sub->print(level);
	}
	for (auto &s: stmts) {
		indent(level);
		s->print();
		putchar('\n');
	}
	level--;
	indent(level);
	printf("}\n");
}
