#include <algorithm>
#include <cassert>
#include <cstring>
#include "ebnf.h"

using namespace std;

set<Symbol*> first_of_production(Symbol *nterm, const Choice &body);
void print_symbol(bool rich, Symbol *s, FILE *fp);

#include "preamble.inc"

static void print_param_list(const vector<Param> &list)
{
	for (const Param &p: list)
		printf("%s &%s, ", p.type.c_str(), p.name.c_str());
}

static void print_params(Symbol *nterm)
{
	print_param_list(nterm->params.out);
	print_param_list(nterm->params.in);
}

static void print_arg_list(const vector<string> &list)
{
	for (const string &arg: list)
		printf("%s, ", arg.c_str());
}

static void print_args(const ArgSpec &args)
{
	print_arg_list(args.out);
	print_arg_list(args.in);
}

static void emit_proc_param_list(Symbol *nterm)
{
	putchar('(');
	print_params(nterm);
	printf("set &&t, set &&f)");
}

static void emit_proc_header(Symbol *nterm)
{
	printf("static bool %s", nterm->name.c_str());
	emit_proc_param_list(nterm);
}

static string term_sv(Symbol *term)
{
	return term->params.out.empty() ? "" : "tokval."+term->params.out[0].name;
}

map<const Choice*, vector<Param>> choice_locals;

static void emit_proc(Symbol *nterm, int level)
{
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
	auto gen_choice = [&](Symbol *nterm, const Choice &choice, const char *ctl) {
		set<Symbol*> f = first_of_production(nterm, choice);
		if (ctl) {
			indent();
			printf("%s (", ctl);
			auto it = f.begin();
			while (it != f.end()) {
				Symbol *term = *it;
				printf("sym == %s", term->name.c_str());
				if (term->sp)
					printf(" && %s(%s)", term->sp, term_sv(term->inner).c_str());
				if (next(it) != f.end())
					printf(" || ");
				it++;
			}
			printf(") {\n");
			use_getsym = true;
			level++;
		}
		// declare locals
		if (choice_locals.count(&choice)) {
			for (const Param &p: choice_locals[&choice]) {
				indent();
				printf("%s %s;\n", p.type.c_str(), p.name.c_str());
			}
		}
		bool ret_true = true;
		for (auto it = choice.begin(); it != choice.end(); it++) {
			Instance *inst = *it;
			Symbol *s = inst->sym;
			indent();
			if (s->kind == Symbol::TERM && use_getsym) {
				printf("getsym();\n");
			} else if (s->kind == Symbol::ACTION) {
				if (s->action) {
					printf("{%s}\n", s->action);
				} else {
					if (inst->args) {
						const vector<string> &out_args = inst->args->out;
						size_t n_out = out_args.size();
						if (n_out) {
							if (n_out > 1) {
								fprintf(stderr, "error: more than one output argument passed to @%s", s->name.c_str());
							}
							printf("%s = ", out_args[0].c_str());
						}
					}
					printf("%s(", s->name.c_str());
					if (inst->args) {
						const vector<string> &in_args = inst->args->in;
						size_t n = in_args.size();
						for (size_t i=0; i<n; i++) {
							printf("%s", in_args[i].c_str());
							if (i < n-1)
								printf(", ");
						}
					}
					printf(");\n");
				}
			} else {
				if (next(it) == choice.end()) {
					if (s->kind == Symbol::TERM) {
						printf("return expect(%s, std::move(t), std::move(f));\n", s->name.c_str());
					} else /* NTERM */ {
						if (s == nterm) {
							printf("goto start;\n");
						} else {
							printf("return ");
							if (s->opening_sym())
								emit_proc(s, level);
							else
								printf("%s", s->name.c_str());
							putchar('(');
							if (inst->args)
								print_args(*inst->args);
							printf("std::move(t), std::move(f));\n");
						}
					}
					ret_true = false;
				} else /* next(it) != choice.end() */ {
					if (s->kind == Symbol::TERM) {
						printf("if (!expect(%s, ", s->name.c_str());
					} else /* NTERM */ {
						printf("if (!");
						if (s->opening_sym())
							emit_proc(s, level);
						else
							printf("%s", s->name.c_str());
						putchar('(');
						if (inst->args)
							print_args(*inst->args);
					}
					set<Symbol*> nt;
					bool thru;
					if (s->kind == Symbol::NTERM || s->weak) {
						auto it2 = next(it);
						while (it2 != choice.end()) {
							Symbol *s2 = (*it2)->sym;
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
					// save token value
					if (s->kind == Symbol::TERM && inst->args) {
						size_t n_out = inst->args->out.size();
						if (n_out) {
							if (n_out > 1) {
								fprintf(stderr, "error: more than one output argument passed to %s", s->name.c_str());
							}
							indent();
							printf("%s = %s;\n", inst->args->out[0].c_str(), term_sv(s).c_str());
						}
					}
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
	if (level) {
		printf("[&]");
		emit_proc_param_list(nterm);
	} else {
		putchar('\n');
		indent();
		emit_proc_header(nterm);
	}
	printf(" {\n");
	level++;
	// emit start label if necessary
	for (auto &choice: nterm->choices) {
		for (Instance *inst: choice) {
			Symbol *s = inst->sym;
			if (s == nterm) {
				printf("start:\n");
				goto gen_body;
			}
		}
	}
gen_body:
	for (auto it = nterm->choices.begin(); it != nterm->choices.end(); it++)
		gen_choice(nterm, *it, next(it) == nterm->choices.end() ? nullptr : "if");
	level--;
	indent();
	putchar('}');
	putchar(level ? ' ' : '\n');
}

static void f(vector<Param> &argtype, size_t pos, const Choice &choice, Symbol *lhs)
{
	if (pos == choice.size())
		return;
	assert (pos<choice.size());
	Instance *inst = choice[pos];
	Symbol *sym = inst->sym;
	size_t mark = argtype.size();
	if (sym->kind == Symbol::NTERM && sym->opening_sym() && lhs != sym)
		for (const Choice &c: sym->choices)
			f(argtype, 0, c, sym);
	if (inst->args) {
		size_t n = inst->args->out.size();
		if (sym->params.out.size() == n) {
			for (size_t i=0; i<n; i++) {
				const string &arg_out = inst->args->out[i];
				auto it = find_if(argtype.rbegin(), argtype.rend(), [&](const Param &p){return p.name == arg_out;});
				if (it == argtype.rend()) {
					/* not found */
					fputc(10, stderr);
					const string &type = sym->params.out[i].type;
					Param p(type, arg_out);
					argtype.emplace_back(p);
					choice_locals[&choice].emplace_back(p);
				}
			}
		} else {
			fprintf(stderr, "error: ");
			print_symbol(false, sym, stderr);
			fprintf(stderr, " expects %lu output arguments, got %lu\n",
				n, sym->params.out.size());
		}
	}
	f(argtype, pos+1, choice, lhs);
	argtype.erase(argtype.begin()+mark, argtype.end());
}

void compute_locals()
{
	for_each_reachable_nterm([&](Symbol *nterm) {
		if (nterm->opening_sym())
			return;
		vector<Param> argtype;
		for (const Param &p: nterm->params.in)
			argtype.emplace_back(p.type, p.name);
		for (const Param &p: nterm->params.out)
			argtype.emplace_back(p.type, p.name);
		size_t n1 = argtype.size();
		for (const Choice &choice: nterm->choices) {
			f(argtype, 0, choice, nterm);
			assert(argtype.size() == n1);
		}
	});
}

void generate_rd()
{
	fputs(preamble1, stdout);
	{
		int n_named_terms = 0;
		for_each_term([&](Symbol *term) {
			if (term->name[0] != '\'')
			n_named_terms++;
		});
		printf("typedef bitset<%d> set;\n", 256+n_named_terms);
	}
	putchar('\n');
	fputs(preamble2, stdout);
	putchar('\n');
	// forward declarations of parsing routines
	for_each_reachable_nterm([](Symbol *nterm) {
		if (!nterm->opening_sym()) {
			emit_proc_header(nterm);
			printf(";\n");
		}
	});
	compute_locals();
	for_each_term([](Symbol *term) {
		if (term->kind == Symbol::ACTION && !term->action) {
			size_t n = term->params.in.size();
			for (size_t i=0; i<n; i++) {
				const Param &p = term->params.in[i];
				printf("%s &%s", p.type.c_str(), p.name.c_str());
				if (i<n-1) printf(", ");
			}
		}
	});
	putchar('\n');
#if 1
	printf("bool parse()\n"
	       "{\n"
	       "\tgetsym();\n"
	       "\treturn %s(set{0}, set{});\n"
	       "}\n",
	       top->name.c_str());
#endif
	for_each_reachable_nterm([](Symbol *nterm) {
		if (!nterm->opening_sym())
			emit_proc(nterm, 0);
	});
}
