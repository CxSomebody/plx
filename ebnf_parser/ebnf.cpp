#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <getopt.h>
#include "ebnf.h"

using namespace std;

// symbol information

struct Symbol;

map<string, Symbol*> term_dict, nterm_dict, action_dict;
Symbol *top;
Symbol *empty;

int Symbol::opening_sym() {
	if (name[0] != '_')
		return 0;
	switch (name[1]) {
	case 's': return '(';
	case 'o': return '[';
	case 'm': return '{';
	}
	assert(0);
}
Symbol::Symbol(SymbolKind kind, const string &name):
	kind(kind), name(name) {}
Symbol::~Symbol() { delete sp; }
Instance::Instance(Symbol *sym, ArgSpec &&args):
	sym(sym), args(make_unique<ArgSpec>(std::move(args))) {}
Instance::Instance(Symbol *sym): sym(sym) {}

Param::Param(const std::string &type, const std::string &name):
	type(type), name(name) {}

void for_each_reachable_nterm(std::function<void(Symbol*)> f)
{
	struct Visitor {
		set<Symbol*> vis;
		std::function<void(Symbol*)> f;
		void visit(Symbol *nterm) {
			assert(nterm->kind == Symbol::NTERM);
			if (vis.count(nterm))
				return;
			vis.insert(nterm);
			f(nterm);
			for (auto &choice: nterm->choices) {
				for (Instance *inst: choice) {
					Symbol *s = inst->sym;
					if (s->kind == Symbol::NTERM)
						visit(s);
				}
			}
		}
		Visitor(std::function<void(Symbol*)> f): f(f) {}
	} visitor(f);
	visitor.visit(top);
}

void print_symbol(bool rich, Symbol *s, FILE *fp);

void print_production(bool rich, Symbol *nterm, const Choice &body, FILE *fp)
{
	print_symbol(rich, nterm, fp);
	fputs(" ->", fp);
	for (Instance *inst: body) {
		Symbol *s = inst->sym;
		fputc(' ', fp);
		print_symbol(rich, s, fp);
	}
	fputc('\n', fp);
}

static void define_empty()
{
	string name("empty");
	empty = nterm_dict[name] = new Symbol(Symbol::NTERM, name);
	empty->choices.emplace_back();
	empty->nullable = true;
	empty->defined = true;
}

static void undef_empty()
{
	nterm_dict.erase("empty");
}
	
static void check_undefined()
{
	bool err = false;
	for_each_nterm([&](Symbol *nterm) {
		if (nterm->choices.empty()) {
			fprintf(stderr, "error: <%s> is undefined\n", nterm->name.c_str());
			err = true;
		}
	});
	if (err)
		exit(1);
}

void print_body(bool rich, const vector<Choice> &choices, FILE *fp);
void print_choice(bool rich, const Choice &choice, FILE *fp);

int closing_sym(int opening)
{
	switch (opening) {
	case '(': return ')';
	case '[': return ']';
	case '{': return '}';
	}
	assert(0);
}

void print_symbol(bool rich, Symbol *s, FILE *fp)
{
	switch (s->kind) {
	case Symbol::TERM:
		fputs(s->name.c_str(), fp);
		if (s->sp)
			fprintf(fp, "?%s", s->sp);
		break;
	case Symbol::NTERM:
		{
			int opening = s->opening_sym();
			if (opening) {
				fprintf(fp, "%c ", opening);
				print_body(rich, *s->choices_core, fp);
				fprintf(fp, " %c", closing_sym(opening));
			} else {
				fprintf(fp, "<%s>", s->name.c_str());
			}
		}
		break;
	case Symbol::ACTION:
		if (s->action) {
			fprintf(fp, "@{%s}", s->action);
		} else {
			fprintf(fp, "@%s", s->name.c_str());
		}
		break;
	default:
		assert(0);
	}
}

void print_choice(bool rich, const Choice &choice, FILE *fp)
{
	auto print_arg_list = [=](const vector<string> &list) {
		size_t n = list.size();
		if (n == 1) {
			fputs(list[0].c_str(), fp);
		} else {
			fputc('(', fp);
			for (size_t i=0; i<n; i++) {
				fputs(list[i].c_str(), fp);
				if (i<n-1) fprintf(fp, ", ");
			}
			fputc(')', fp);
		}
	};
	bool sep = false;
	for (Instance *inst: choice) {
		if (!rich && inst->sym->kind == Symbol::ACTION)
			continue;
		if (sep)
			fputc(' ', fp);
		sep = true;
		print_symbol(rich, inst->sym, fp);
		if (rich && inst->args) {
			if (!inst->args->in.empty()) {
				fputs("↓", fp);
				print_arg_list(inst->args->in);
			}
			if (!inst->args->out.empty()) {
				fputs("↑", fp);
				print_arg_list(inst->args->out);
			}
		}
	}
}

void print_body(bool rich, const vector<Choice> &choices, FILE *fp)
{
	size_t n = choices.size();
	for (size_t i=0; i<n; i++) {
		print_choice(rich, choices[i], fp);
		if (i < n-1)
			fputs(" | ", fp);
	}
}

void print_rules(bool rich, FILE *fp)
{
	auto print_param = [=](const Param &p) {
		fprintf(fp, "<%s> %s", p.type.c_str(), p.name.c_str());
	};
	auto print_param_list = [=](const std::vector<Param> &list) {
		size_t n = list.size();
		if (n == 1) {
			print_param(list[0]);
		} else {
			fputc('(', fp);
			for (size_t i=0; i<n; i++) {
				print_param(list[i]);
				if (i < n-1)
					fputs(", ", fp);
			}
			fputc(')', fp);
		}
	};
	for_each_reachable_nterm([=](Symbol *nterm) {
		if (!nterm->opening_sym()) {
			fprintf(fp, "<%s>", nterm->name.c_str());
			if (rich && !(nterm->params.out.empty() && nterm->params.in.empty())) {
				if (!nterm->params.in.empty()) {
					fputs("↓", fp);
					print_param_list(nterm->params.in);
				}
				if (!nterm->params.out.empty()) {
					fputs("↑", fp);
					print_param_list(nterm->params.out);
				}
			}
			fprintf(fp, " ::= ");
			print_body(rich, nterm->choices, fp);
			fputc('\n', fp);
		}
	});
}

void print_term_set(const set<Symbol*> &s)
{
	putchar('{');
	for (Symbol *term: s)
		printf(" %s", term->name.c_str());
	printf(" }");
}

void print_first_follow()
{
	for_each_nterm([](Symbol *nterm) {
		print_symbol(false, nterm, stdout);
		putchar('\n');
		printf("FIRST: ");
		print_term_set(nterm->first);
		putchar('\n');
		printf("FOLLOW: ");
		print_term_set(nterm->follow);
		putchar('\n');
	});
}

static void list_symbols()
{
	printf("terminals: %lu\n", term_dict.size());
	for_each_term([](Symbol *term) {
		puts(term->name.c_str());
	});
	printf("nterminals: %lu\n", nterm_dict.size());
	for_each_nterm([](Symbol *nterm) {
		print_symbol(false, nterm, stdout);
		putchar('\n');
	});
}

#if 0
int term_id = 1; // 0 for EOF
static void number_terms()
{
	for_each_term([&](Symbol *term) {
		term->id = term_id++;
	});
}
#endif

void parse();

void check_grammar();
void compute_first_follow();

void generate_rd();

extern FILE *yyin;
static enum {
	PRINT_RULES,
	LIST_SYMBOLS,
	CHECK_GRAMMAR,
	PRINT_FIRST_FOLLOW,
	GENERATE_PARSER,
} action;
static bool rich; // print ATG rather than plain EBNF

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s (-c|-f|-g|-l|-p|-P) <file>\n", progname);
	exit(2);
}

int main(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "cfglpP")) != -1) {
		switch (opt) {
		case 'c':
			action = CHECK_GRAMMAR;
			break;
		case 'f':
			action = PRINT_FIRST_FOLLOW;
			break;
		case 'g':
			action = GENERATE_PARSER;
			break;
		case 'l':
			action = LIST_SYMBOLS;
			break;
		case 'P':
			rich = true;
			/* fallthrough */
		case 'p':
			action = PRINT_RULES;
			break;
		default:
			usage(argv[0]);
		}
	}
	if (optind >= argc)
		usage(argv[0]);
	char *fpath = argv[optind];
	if (!(yyin = fopen(fpath, "r"))) {
		perror(fpath);
		exit(1);
	}
	define_empty();
	parse();
	undef_empty();
	if (!top) {
		fputs("error: grammar is empty\n", stderr);
		exit(1);
	}
	check_undefined();
	switch (action) {
	case PRINT_RULES:
		print_rules(rich, stdout);
		break;
	case LIST_SYMBOLS:
		list_symbols();
		break;
	case CHECK_GRAMMAR:
		compute_first_follow();
		check_grammar();
		break;
	case PRINT_FIRST_FOLLOW:
		compute_first_follow();
		print_first_follow();
		break;
	case GENERATE_PARSER:
		compute_first_follow();
		check_grammar();
		//number_terms();
		generate_rd();
		break;
	default:
		assert(0);
	}
	return 0;
}
