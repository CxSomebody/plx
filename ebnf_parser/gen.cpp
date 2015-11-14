#include <cassert>
#include <cstring>
#include "ebnf.h"

using namespace std;

set<Symbol*> first_of_production(Symbol *nterm, const vector<Symbol*> &body);

void emit_proc_header(const char *name)
{
	printf("void %s(std::set<int> &&follow)", name);
}

void generate_parsing_routine(Symbol *nterm, const vector<vector<Symbol*>> &choices, int level, bool use_getsym)
{
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
			if (strcmp(ctl, "else")) {
				print_cond(f);
				use_getsym = true;
			}
			printf(" {\n");
			level++;
		}
		for (auto it = choice.begin(); it != choice.end(); it++) {
			Symbol *s = *it;
			switch (s->kind) {
				int opening;
			case Symbol::TERM:
				indent();
				if (use_getsym) printf("getsym();\n");
				else printf("if (!expect(%s, follow)) return;\n", s->name.c_str());
				use_getsym = false;
				break;
			case Symbol::NTERM:
				if ((opening = s->opening_sym())) {
					const char *sctl;
					switch (opening) {
					case '(': sctl = nullptr; break;
					case '[': sctl = "if"; break;
					case '{': sctl = "while"; break;
					default: assert(0);
					}
					if (sctl) {
						indent();
						printf("%s ", sctl);
						print_cond(s->first);
						printf(" {\n");
						use_getsym = true;
						level++;
					}
					generate_parsing_routine(s, *s->choices_core, level, use_getsym);
					use_getsym = false;
					if (sctl) {
						level--;
						indent();
						printf("}\n");
					}
					break;
				} else {
					indent();
					printf("%s(set_union(follow, std::set<int>", s->name.c_str());
					if (next(it) == choice.end())
						print_set(nterm->follow);
					else
						print_set((*next(it))->first);
					printf("));\n");
					//printf("%s();\n", s->name.c_str());
					use_getsym = false;
				}
				break;
			default:
				assert(0);
			}
		}
		if (ctl) {
			level--;
			indent();
			printf("}\n");
		}
	};
	if (!nterm->opening_sym()) {
		putchar('\n');
		indent();
		emit_proc_header(nterm->name.c_str());
		printf(" {\n");
		level++;
	}
	if (choices.size() == 1) {
		gen_choice(nterm, choices[0], nullptr);
	} else if (choices.size()) /* more than one choice */ {
		gen_choice(nterm, choices[0], "if");
		auto it = next(choices.begin());
		while (it != choices.end()) {
			if (next(it) == choices.end()) {
				if (!it->empty())
					gen_choice(nterm, *it, "else");
			} else {
				gen_choice(nterm, *it, "else if");
			}
			it++;
		}
	}
	if (!nterm->opening_sym()) {
		level--;
		indent();
		printf("}\n");
	}
}
