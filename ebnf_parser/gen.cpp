#include <cassert>
#include <cstring>
#include "ebnf.h"

using namespace std;

set<Symbol*> first_of_production(Symbol *nterm, const vector<Symbol*> &body);

void generate_parsing_routine(Symbol *nterm, const vector<vector<Symbol*>> &choices, int level, bool use_getsym)
{
	auto indent = [&]() {
		for (int i=0; i<level; i++)
			putchar('\t');
	};
	auto print_cond = [](const set<Symbol*> f) {
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
	auto gen_choice = [&](Symbol *nterm, const vector<Symbol*> &choice, const char *ctl) {
		set<Symbol*> f = first_of_production(nterm, choice);
		if (ctl) {
			indent();
			if (strcmp(ctl, "else")) {
				printf("%s ", ctl);
				print_cond(f);
				printf(" {\n");
				use_getsym = true;
			} else {
				printf("else {\n");
			}
			level++;
		}
		for (Symbol *s: choice) {
			switch (s->kind) {
				int opening;
			case Symbol::TERM:
				indent();
				if (use_getsym) printf("getsym();\n");
				else printf("expect(%s);\n", s->name.c_str());
				use_getsym = false;
				break;
			case Symbol::NTERM:
				if ((opening = s->opening_sym())) {
					switch (opening) {
						const char *sctl;
					case '(':
						generate_parsing_routine(s, *s->choices_core, level, use_getsym);
						use_getsym = false;
						break;
					case '[':
					case '{':
						switch (opening) {
						case '[': sctl = "if"; break;
						case '{': sctl = "while"; break;
						default: assert(0);
						}
						indent();
						printf("%s ", sctl);
						print_cond(s->first);
						printf(" {\n");
						use_getsym = true;
						level++;
						generate_parsing_routine(s, *s->choices_core, level, use_getsym);
						use_getsym = false;
						level--;
						indent();
						printf("}\n");
						break;
					default:
						assert(0);
					}
				} else {
					indent();
					printf("%s();\n", s->name.c_str());
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
		indent();
		printf("void %s() {\n", nterm->name.c_str());
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
