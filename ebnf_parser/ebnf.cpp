#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <getopt.h>
#include "ebnf.h"

using namespace std;

// symbol information

struct Symbol;

map<string, Symbol*> term_dict, nterm_dict;
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
				for (Symbol *s: choice) {
					if (s->kind == Symbol::NTERM)
						visit(s);
				}
			}
		}
		Visitor(std::function<void(Symbol*)> f): f(f) {}
	} visitor(f);
	visitor.visit(top);
}

void print_symbol(Symbol *s, FILE *fp);

void print_production(Symbol *nterm, const vector<Symbol*> &body, FILE *fp)
{
	print_symbol(nterm, fp);
	fputs(" ->", fp);
	for (Symbol *s: body) {
		fputc(' ', fp);
		print_symbol(s, fp);
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

void print_body(const vector<vector<Symbol*>> &choices, FILE *fp);
void print_choice(const vector<Symbol*> &choice, FILE *fp);

int closing_sym(int opening)
{
	switch (opening) {
	case '(': return ')';
	case '[': return ']';
	case '{': return '}';
	}
	assert(0);
}

void print_symbol(Symbol *s, FILE *fp)
{
	switch (s->kind) {
	case Symbol::TERM:
		fputs(s->name.c_str(), fp);
		if (s->sp)
			fprintf(fp, "::%s", s->sp);
		break;
	case Symbol::NTERM:
		{
			int opening = s->opening_sym();
			if (opening) {
				fprintf(fp, "%c ", opening);
				print_body(*s->choices_core, fp);
				fprintf(fp, " %c", closing_sym(opening));
			} else {
				fprintf(fp, "<%s>", s->name.c_str());
			}
		}
		break;
	case Symbol::ACTION:
		fprintf(fp, "@%s", s->name.c_str());
		break;
	default:
		assert(0);
	}
}

void print_choice(const vector<Symbol*> &choice, FILE *fp)
{
	size_t n = choice.size();
	for (size_t i=0; i<n; i++) {
		print_symbol(choice[i], fp);
		if (i < n-1)
			fputc(' ', fp);
	}
}

void print_body(const vector<vector<Symbol*>> &choices, FILE *fp)
{
	size_t n = choices.size();
	for (size_t i=0; i<n; i++) {
		print_choice(choices[i], fp);
		if (i < n-1)
			fputs(" | ", fp);
	}
}

void print_rules(FILE *fp)
{
	for_each_reachable_nterm([=](Symbol *nterm) {
		if (!nterm->opening_sym()) {
			printf("<%s> ::= ", nterm->name.c_str());
			print_body(nterm->choices, fp);
			putchar('\n');
		}
	});
}

void print_symbol_set(const char *set_name, const char *nterm_name, const set<Symbol*> &s)
{
	printf("%s(<%s>) = {", set_name, nterm_name);
	for (Symbol *term: s)
		printf(" %s", term->name.c_str());
	puts(" }");
}

void print_first_follow()
{
	for_each_nterm([](Symbol *nterm) {
		if (!nterm->opening_sym()) {
			const char *nterm_name = nterm->name.c_str();
			print_symbol_set("FIRST", nterm_name, nterm->first);
			print_symbol_set("FOLLOW", nterm_name, nterm->follow);
		}
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
		print_symbol(nterm, stdout);
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

void emit_proc_header(Symbol *nterm);
void emit_proc(Symbol *nterm, const vector<vector<Symbol*>> &choices);

#include "preamble.inc"

extern FILE *yyin;
static enum {
	PRINT_RULES,
	LIST_SYMBOLS,
	CHECK_GRAMMAR,
	PRINT_FIRST_FOLLOW,
	GENERATE_PARSER,
} action;

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s (-c|-f|-g|-l|-p) <file>\n", progname);
	exit(2);
}

int main(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "cfglp")) != -1) {
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
		print_rules(stdout);
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
		for_each_reachable_nterm([](Symbol *nterm) {
			emit_proc_header(nterm);
			printf(";\n");
		});
		putchar('\n');
		printf("bool parse()\n"
		       "{\n"
		       "\tgetsym();\n"
		       "\treturn %s(set{0}, set{});\n"
		       "}\n",
		       top->name.c_str());
		for_each_reachable_nterm([](Symbol *nterm) {
			emit_proc(nterm, nterm->choices);
		});
		break;
	default:
		assert(0);
	}
	return 0;
}
