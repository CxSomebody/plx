#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include "ebnf.h"

using namespace std;

// symbol information

struct Symbol;

map<string, Symbol*> term_dict, nterm_dict;
Symbol *top;
Symbol *empty;

int Symbol::opening_sym() {
	char opening = name[0];
	switch (opening) {
	case '(': case '[': case '{':
		return opening;
	}
	return 0;
}
Symbol::Symbol(SymbolKind kind, const string &name):
	kind(kind), name(name) {}

void NTermVisitor::visit(Symbol *nterm) {
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
NTermVisitor::NTermVisitor(function<void(Symbol*)> f): f(f) {}

void print_symbol(Symbol *s);

void print_production(Symbol *nterm, const vector<Symbol*> &body)
{
	print_symbol(nterm);
	fputs(" ->", stdout);
	for (Symbol *s: body) {
		putchar(' ');
		print_symbol(s);
	}
	putchar('\n');
}

void define_empty()
{
	string name("empty");
	empty = nterm_dict[name] = new Symbol(Symbol::NTERM, name);
	empty->choices.emplace_back();
	empty->nullable = true;
	empty->defined = true;
}

void undef_empty()
{
	nterm_dict.erase("empty");
}
	
void check_undefined()
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

void print_body(const vector<vector<Symbol*>> &choices);
void print_choice(const vector<Symbol*> &choice);

int closing_sym(int opening)
{
	switch (opening) {
	case '(': return ')';
	case '[': return ']';
	case '{': return '}';
	}
	assert(0);
}

void print_symbol(Symbol *s)
{
	switch (s->kind) {
	case Symbol::TERM:
		fputs(s->name.c_str(), stdout);
		break;
	case Symbol::NTERM:
		{
			int opening = s->opening_sym();
			if (opening) {
				printf("%c ", opening);
				print_body(*s->choices_core);
				printf(" %c", closing_sym(opening));
			} else {
				printf("<%s>", s->name.c_str());
			}
		}
		break;
	default:
		assert(0);
	}
}

void print_choice(const vector<Symbol*> &choice)
{
	size_t n = choice.size();
	for (size_t i=0; i<n; i++) {
		print_symbol(choice[i]);
		if (i < n-1)
			putchar(' ');
	}
}

void print_body(const vector<vector<Symbol*>> &choices)
{
	size_t n = choices.size();
	for (size_t i=0; i<n; i++) {
		print_choice(choices[i]);
		if (i < n-1)
			fputs(" | ", stdout);
	}
}

void print_rules()
{
	NTermVisitor([](Symbol *nterm) {
		if (!nterm->opening_sym()) {
			printf("<%s> ::= ", nterm->name.c_str());
			print_body(nterm->choices);
			putchar('\n');
		}
	}).visit(top);
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

void list_symbols()
{
	printf("terminals: %lu\n", term_dict.size());
	for_each_term([](Symbol *term) {
		puts(term->name.c_str());
	});
	printf("nonterminals: %lu\n", nterm_dict.size());
	for_each_nterm([](Symbol *nterm) {
		print_symbol(nterm);
		putchar('\n');
	});
}

void parse();

void check_grammar();
void compute_first_follow();

void generate_parsing_routine(Symbol *nterm, const vector<vector<Symbol*>> &choices, int level, bool use_getsym);

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
	fprintf(stderr, "usage: %s {-c|-f|-l|-p} file\n", progname);
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
		print_rules();
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
		NTermVisitor([](Symbol *nterm) {
			if (!nterm->opening_sym())
				generate_parsing_routine(nterm, nterm->choices, 0, false);
		}).visit(top);
		break;
	default:
		assert(0);
	}
	return 0;
}
