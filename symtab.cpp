#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

Type::~Type() {}
Type::Type(Kind kind): kind(kind) {}
IntType::IntType(): Type(INT) {}
CharType::CharType(): Type(CHAR) {}
ArrayType::ArrayType(Type *elemtype, int size): Type(ARRAY), elemtype(elemtype), size(size) {}
Symbol::Symbol(SymbolKind kind, const string &name): kind(kind), name(name) {}

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

Symbol *var_symbol(const string &name, Type *type)
{
	Symbol *s = new Symbol(Symbol::VAR, name);
	s->type = type;
	return s;
}

Symbol *const_symbol(const string &name, int val)
{
	Symbol *s = new Symbol(Symbol::CONST, name);
	s->val = val;
	return s;
}

Symbol *proc_symbol(const string &name, Type *rettype)
{
	Symbol *s = new Symbol(Symbol::PROC, name);
	s->rettype = rettype;
	return s;
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
		symtab->map[name] = const_symbol(name, val);
	}
}

void def_vars(const vector<string> &names, Type *type)
{
	for (const string &name: names)
		if (check_redef(name))
			symtab->map[name] = var_symbol(name, type);
}

void def_func(const ProcHeader &header, Type *rettype)
{
	const string &name = header.first;
	if (check_redef(name)) {
		symtab->map[name] = proc_symbol(name, rettype);
	}
}

void def_params(const vector<Param> &params)
{
	for (const Param &p: params) {
		const string &name = p.name;
		if (check_redef(name)) {
			symtab->map[name] = var_symbol(name, p.type);
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

bool is_proc(const string &name)
{
	Symbol *s = lookup(name);
	return s && s->kind == Symbol::PROC && !s->rettype;
}

vector<Param> param_group(vector<string> &&names, Type *type, bool byref)
{
	vector<Param> params;
	for (string &name: names)
		params.emplace_back(move(name), type, byref);
	return params;
}

void IntType::print()
{
	printf("int");
}

void CharType::print()
{
	printf("char");
}

void ArrayType::print()
{
	printf("array[%d] of ", size);
	elemtype->print();
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
			s->type->print();
		} else if (s->kind == Symbol::CONST) {
			printf(" %d", s->val);
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

void translate(unique_ptr<Block> &&blk)
{
	blk->print(0);
}
