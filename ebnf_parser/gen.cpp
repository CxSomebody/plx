#include <cassert>
#include <cstring>
#include "ebnf.h"

using namespace std;

set<Symbol*> first_of_production(Symbol *nterm, const vector<Symbol*> &body);

void emit_proc_header(Symbol *nterm)
{
	printf("static bool %s(std::set<int> &&t, std::set<int> &&f)", nterm->name.c_str());
}

void emit_proc(Symbol *nterm, const vector<vector<Symbol*>> &choices)
{
	int level = 0;
	bool use_getsym = false;
	auto indent = [&]() {
		for (int i=0; i<level; i++)
			putchar('\t');
	};
	auto print_cond = [](const set<Symbol*> f) {
		putchar(' ');
		putchar('(');
		auto it = f.begin();
		while (it != f.end()) {
			Symbol *term = *it;
			printf("sym == %s", term->name.c_str());
			if (next(it) != f.end())
				printf(" || ");
			it++;
		}
		putchar(')');
	};
	auto print_set = [](const set<Symbol*> f) {
		putchar('{');
		auto it = f.begin();
		while (it != f.end()) {
			Symbol *term = *it;
			printf("%s", term->name.c_str());
			if (next(it) != f.end())
				putchar(',');
			it++;
		}
		putchar('}');
	};
	auto gen_choice = [&](Symbol *nterm, const vector<Symbol*> &choice, const char *ctl) {
		set<Symbol*> f = first_of_production(nterm, choice);
		if (ctl) {
			indent();
			printf("%s", ctl);
			print_cond(f);
			use_getsym = true;
			printf(" {\n");
			level++;
		}
		bool ret_true = true;
		for (auto it = choice.begin(); it != choice.end(); it++) {
			Symbol *s = *it;
			indent();
			if (s->kind == Symbol::TERM && use_getsym) {
				printf("getsym();\n");
			} else {
				if (next(it) == choice.end()) {
					if (s->kind == Symbol::TERM) {
						printf("return expect(%s, std::move(t), std::move(f));\n", s->name.c_str());
					} else {
						printf("return %s(std::move(t), std::move(f));\n", s->name.c_str());
					}
					ret_true = false;
				} else {
					if (s->kind == Symbol::TERM) {
						printf("if (!expect(%s, std::set<int>{}", s->name.c_str());
					} else {
						printf("if (!%s(std::set<int>", s->name.c_str());
						print_set((*next(it))->first);
					}
					printf(", set_union(t, f))) return t.count(sym);\n");
				}
			}
			use_getsym = false;
		}
		if (ret_true) {
			indent();
			printf("return true;\n");
		}
		if (ctl) {
			level--;
			indent();
			printf("}\n");
		}
	};
	putchar('\n');
	indent();
	emit_proc_header(nterm);
	printf(" {\n");
	level++;
	for (auto it = choices.begin(); it != choices.end(); it++) {
		if (next(it) == choices.end()) {
			gen_choice(nterm, *it, nullptr);
		} else {
			gen_choice(nterm, *it, "if");
		}
	}
	level--;
	indent();
	printf("}\n");
}
