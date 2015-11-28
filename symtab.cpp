#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

Symbol::Symbol(Kind kind, const string &name): kind(kind), name(name) {}

void Symbol::print()
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

VarSymbol::VarSymbol(const string &name, Type *type): Symbol(Symbol::VAR, name), type(type) {}
ConstSymbol::ConstSymbol(const string &name, int val): Symbol(Symbol::CONST, name), val(val) {}
ProcSymbol::ProcSymbol(const string &name, Type *rettype): Symbol(Symbol::PROC, name), rettype(rettype) {}

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

void def_vars(const vector<string> &names, Type *type)
{
	for (const string &name: names)
		if (check_redef(name))
			symtab->map[name] = new VarSymbol(name, type);
}

void def_func(const ProcHeader &header, Type *rettype)
{
	const string &name = header.first;
	if (check_redef(name)) {
		symtab->map[name] = new ProcSymbol(name, rettype);
	}
}

void def_params(const vector<Param> &params)
{
	for (const Param &p: params) {
		const string &name = p.name;
		if (check_redef(name)) {
			symtab->map[name] = new VarSymbol(name, p.type);
		}
	}
}

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

void print_symtab(SymbolTable *symtab)
{
	assert(symtab);
	for (auto &pair: symtab->map) {
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
			static_cast<VarSymbol*>(s)->type->print();
		} else if (s->kind == Symbol::CONST) {
			printf(" %d", static_cast<ConstSymbol*>(s)->val);
		}
		putchar('\n');
	}
}

void push_symtab()
{
	SymbolTable *newst = new SymbolTable();
	newst->up = symtab;
	symtab = newst;
}

SymbolTable *pop_symtab()
{
	SymbolTable *savedst = symtab;
	symtab = symtab->up;
	return savedst;
}

Block::Block(string &&name, vector<unique_ptr<Block>> &&subs, vector<unique_ptr<Stmt>> &&stmts, SymbolTable *symtab):
	name(move(name)), subs(move(subs)), stmts(move(stmts)), symtab(symtab)
{
}

void print_stmt(Stmt *s);

void Block::print(int level)
{
	auto indent = [&]() {
		for (int i=0; i<level; i++)
			printf("  ");
	};
	indent();
	printf("%s {\n", name.c_str());
	level++;
	for (auto &sub: subs) {
		sub->print(level);
	}
	for (auto &s: stmts) {
		indent();
		s->print();
		putchar('\n');
	}
	level--;
	indent();
	printf("}\n");
}
