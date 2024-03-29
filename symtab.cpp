#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

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
		symtab->map[name] = new ConstSymbol(name, symtab->level, val);
	}
}

VarSymbol *def_var(const string &name, Type *type)
{
	if (check_redef(name)) {
		VarSymbol *vs = new VarSymbol(name, type, symtab->level, false);
		symtab->map[name] = vs;
		return vs;
	}
	return nullptr;
}

ProcSymbol *def_func(const string &name, const vector<Param> &params, Type *rettype)
{
	if (check_redef(name)) {
		ProcSymbol *ps = new ProcSymbol(name, symtab->proc, params, rettype);
		symtab->map[name] = ps;
		return ps;
	}
	return nullptr;
}

vector<VarSymbol*> def_params(const vector<Param> &params)
{
	// level 0 block has no params, so level > 0
	assert(symtab->level > 0 || params.empty());
	vector<VarSymbol*> ret;
	int offset = 8+(symtab->level-1)*4;
	for (const Param &p: params) {
		const string &name = p.name;
		if (check_redef(name)) {
			VarSymbol *vs = new VarSymbol(name, p.type, symtab->level, p.byref);
			vs->offset = offset;
			symtab->map[name] = vs;
			offset += 4;
			ret.push_back(vs);
		}
	}
	return ret;
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
			VarSymbol *vs = static_cast<VarSymbol*>(s);
			printf(" %s level=%d offset=%+d", vs->type->tostr().c_str(), vs->level, vs->offset);
		} else if (s->kind == Symbol::CONST) {
			printf(" %d", static_cast<ConstSymbol*>(s)->val);
		}
		putchar('\n');
	}
}

void push_symtab(ProcSymbol *proc)
{
	int nextlevel = symtab ? symtab->level+1 : 0;
	SymbolTable *newst = new SymbolTable(symtab, proc, nextlevel);
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
	printf("%s\n", proc->decorated_name.c_str());
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
