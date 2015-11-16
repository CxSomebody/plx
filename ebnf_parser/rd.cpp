#include <cassert>
#include <cstring>
#include "ebnf.h"

using namespace std;

set<Symbol*> first_of_production(Symbol *nterm, const vector<Symbol*> &body);

void emit_proc_header(Symbol *nterm)
{
	printf("static bool %s(set &&t, set &&f)", nterm->name.c_str());
}

void emit_proc(Symbol *nterm, const vector<vector<Symbol*>> &choices)
{
	int level = 0;
	bool use_getsym = false;
	auto indent = [&]() {
		for (int i=0; i<level; i++)
			putchar('\t');
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
			printf("%s (", ctl);
			auto it = f.begin();
			while (it != f.end()) {
				Symbol *term = *it;
				printf("sym == %s", term->name.c_str());
				if (term->sp)
					printf(" && %s()", term->sp);
				if (next(it) != f.end())
					printf(" || ");
				it++;
			}
			printf(") {\n");
			use_getsym = true;
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
						if (s == nterm)
							printf("goto start;\n");
						else
							printf("return %s(std::move(t), std::move(f));\n", s->name.c_str());
					}
					ret_true = false;
				} else {
					if (s->kind == Symbol::TERM) {
						printf("if (!expect(%s, ", s->name.c_str());
					} else {
						printf("if (!%s(", s->name.c_str());
					}
					set<Symbol*> nt;
					bool thru;
					if (s->kind == Symbol::NTERM || s->weak) {
						auto it2 = next(it);
						while (it2 != choice.end()) {
							Symbol *s2 = *it2;
							nt.insert(s2->first.begin(), s2->first.end());
							if (!s2->nullable)
								break;
							it2++;
						}
						thru = it2 == choice.end();
					} else {
						thru = false;
					}
					printf("set");
					print_set(nt);
					if (thru) {
						printf("|t, std::move(f)");
					} else {
						printf(", t|f");
					}
					printf(")) return t.get(sym);\n");
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
	for (auto &choice: nterm->choices) {
		for (Symbol *s: choice) {
			if (s == nterm) {
				printf("start:\n");
				goto gen_body;
			}
		}
	}
gen_body:
	for (auto it = choices.begin(); it != choices.end(); it++)
		gen_choice(nterm, *it, next(it) == choices.end() ? nullptr : "if");
	level--;
	indent();
	printf("}\n");
}
