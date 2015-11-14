#include <cassert>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <getopt.h>
#include "tokens.h"

extern FILE *yyin;
extern char *yytext;
extern int yyleng;
extern int yylineno;
extern char tokval;

extern "C" int yylex();

int sym;

using namespace std;

// symbol information

struct Symbol;

map<string, Symbol*> term_dict, nterm_dict;
Symbol *top;

struct Symbol {
	enum SymbolKind { TERM, NTERM } kind;
	string name;
	vector<vector<Symbol*>> choices, *choices_core = nullptr;
	set<Symbol*> first, follow;
	bool nullable = false;
	bool defined = false;
	bool predefined = false;
	int opening_sym() {
		char opening = name[0];
		switch (opening) {
		case '(': case '[': case '{':
			return opening;
		}
		return 0;
	}
	Symbol(SymbolKind kind, const string &name):
	       	kind(kind), name(name) {}
	~Symbol() {
		delete choices_core;
	}
	Symbol(const Symbol &) = delete;
};

// algorithm for computing FIRST and FOLLOW

bool insert_all(set<Symbol*> &a, const set<Symbol*> &b)
{
	size_t old_size = a.size();
	a.insert(b.begin(), b.end());
	return a.size() > old_size;
}

void compute_first_follow()
{
	for (auto &pair: term_dict) {
		Symbol *term = pair.second;
		term->first.insert(term);
	}
	bool changed;
	do {
		changed = false;
		for (auto &pair: nterm_dict) {
			Symbol *nterm = pair.second;
			for (auto &choice: nterm->choices) {
				if (!nterm->nullable) {
					bool opaque = false;
					for (Symbol *s: choice) {
						if (!s->nullable) {
							opaque = true;
							break;
						}
					}
					if (!opaque) {
						nterm->nullable = true;
						changed = true;
					}
				}
				for (Symbol *s: choice) {
					if (insert_all(nterm->first, s->first))
						changed = true;
					if (!s->nullable)
						break;
				}
				for (auto it1 = choice.begin(); it1 != choice.end(); it1++) {
					Symbol *s1 = *it1, *s2;
					if (s1->kind == Symbol::TERM)
						continue;
					auto it2 = it1+1;
					while (it2 != choice.end()) {
						s2 = *it2;
						if (!s2->nullable)
							break;
						it2++;
					}
					if (it2 == choice.end()) {
						if (insert_all(s1->follow, nterm->follow))
							changed = true;
					} else {
						if (insert_all(s1->follow, s2->first))
							changed = true;
					}
				}
			}
		}
	} while (changed);
}

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

void detect_useless_rules()
{
	struct {
		set<Symbol*> vis;
		void visit(Symbol *nterm) {
			assert(nterm->kind == Symbol::NTERM);
			if (vis.count(nterm))
				return;
			vis.insert(nterm);
			for (auto &choice: nterm->choices) {
				for (Symbol *s: choice) {
					if (s->kind == Symbol::NTERM)
						visit(s);
				}
			}
		}
	} visitor;
	visitor.visit(top);
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		if (!nterm->predefined && !visitor.vis.count(nterm))
			fprintf(stderr, "warning: <%s> is useless\n", nterm->name.c_str());
	}
}

// true if no left recursion
bool check_left_recursion()
{
	struct {
		set<Symbol*> vis;
		bool ans = true;
		void visit(Symbol *nterm) {
			assert(nterm->kind == Symbol::NTERM);
			if (vis.count(nterm)) {
				fprintf(stderr, "error: <%s> is left-recursive\n", nterm->name.c_str());
				ans = false;
				return;
			}
			vis.insert(nterm);
			for (auto &choice: nterm->choices) {
				auto it = choice.begin();
				while (it != choice.end()) {
					Symbol *s = *it;
					if (s->kind == Symbol::NTERM)
						visit(s);
					if (!s->nullable)
						break;
					it++;
				}
			}
		}
	} visitor;
	visitor.visit(top);
	return visitor.ans;
}

bool check_grammar()
{
	// FIRST(X->y) = if nullable(y) then FIRST(y) ∪ FOLLOW(X) else FIRST(y)
	/* for each nonterminal X, for each pair of distinct productions
	   X->y1 and X->y2, FIRST(X->y1) ∩ FIRST(X->y2) = ∅ */
	bool ans = true;
	detect_useless_rules();
	if (!check_left_recursion())
		ans = false;
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		map<Symbol*, vector<Symbol*>*> m;
		for (auto &choice: nterm->choices) {
			set<Symbol*> f;
			auto it = choice.begin();
			while (it != choice.end()) {
				Symbol *s = *it;
				f.insert(s->first.begin(), s->first.end());
				if (!s->nullable)
					break;
				it++;
			}
			if (it == choice.end())
				f.insert(nterm->follow.begin(), nterm->follow.end());
			for (Symbol *s: f) {
				if (m[s]) {
					printf("conflict on %s:\n", s->name.c_str());
					print_production(nterm, *m[s]);
					print_production(nterm, choice);
					ans = false;
				} else {
					m[s] = &choice;
				}
			}
		}
	}
	return ans;
}

// parser routines

void getsym()
{
	sym = yylex();
}

void syntax_error()
{
	fprintf(stderr, "%d: error: syntax error (sym=%d)\n", yylineno, sym);
	exit(1);
}

void parse_body(vector<vector<Symbol*>> &body);

int closing_sym(int opening)
{
	switch (opening) {
	case '(': return ')';
	case '[': return ']';
	case '{': return '}';
	}
	assert(0);
}

void synth_nterm_name(int kind, char *name)
{
	static int id;
	sprintf(name, "%c%d%c", kind, id, closing_sym(kind));
	id++;
}

void parse_seq(vector<Symbol *> &choice)
{
	for (;;) {
		switch (sym) {
		case '(':
		case '[':
		case '{':
			{
				char synth_name[16];
				int opening = sym;
				synth_nterm_name(sym, synth_name);
				string nterm_name(synth_name);
				Symbol *nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				getsym();
				parse_body(nterm->choices);
				if (sym == closing_sym(opening)) getsym();
				else syntax_error();
				switch (opening) {
				case '{':
					nterm->choices_core = new vector<vector<Symbol*>>(nterm->choices);
					for (auto &choice: nterm->choices)
						choice.emplace_back(nterm);
				       	nterm->choices.emplace_back();
					break;
				case '[':
					nterm->choices_core = new vector<vector<Symbol*>>(nterm->choices);
				       	nterm->choices.emplace_back();
					break;
				case '(':
					break;
				default:
					assert(0);
				}
				choice.emplace_back(nterm);
			}
			break;
		case CHAR:
			{
				char synth_name[4];
				sprintf(synth_name, "'%c'", tokval);
				string term_name(synth_name);
				Symbol *term = term_dict[term_name];
				if (!term) {
					term = term_dict[term_name] = new Symbol(Symbol::TERM, term_name);
				}
				choice.emplace_back(term);
				getsym();
			}
			break;
		case NTERM:
			{
				string nterm_name(yytext+1, yytext+yyleng-1);
				Symbol *nterm = nterm_dict[nterm_name];
				if (!nterm) {
					nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				}
				choice.emplace_back(nterm);
				getsym();
			}
			break;
		case IDENT:
			{
				string term_name(yytext);
				Symbol *term = term_dict[term_name];
				if (!term) {
					term = term_dict[term_name] = new Symbol(Symbol::TERM, term_name);
				}
				choice.emplace_back(term);
				getsym();
			}
			break;
		default:
			return;
		}
	}
}

void parse_body(vector<vector<Symbol*>> &body)
{
	for (;;) {
		body.emplace_back();
		parse_seq(body.back());
		if (sym != '|')
			break;
		getsym();
	}
}

void parse_rule()
{
	string nterm_name;
	if (sym == NTERM) {
		nterm_name = string(yytext+1, yytext+yyleng-1);
		getsym();
	} else {
		syntax_error();
	}
	if (sym == IS) getsym();
	else syntax_error();

	Symbol *nterm = nterm_dict[nterm_name];
	if (nterm) {
		if (nterm->defined) {
			fprintf(stderr, "%d: error: redefinition of <%s>\n",
				yylineno, nterm_name.c_str());
			exit(1);
		} else {
			nterm->defined = true;
		}
	} else {
		nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
	}

	if (!top) top = nterm;

	parse_body(nterm->choices);
	if (sym == '\n') getsym();
	else syntax_error();
}

void parse()
{
	getsym();
	while (sym)
		parse_rule();
}

void define_empty()
{
	string name("empty");
	Symbol *empty = nterm_dict[name] = new Symbol(Symbol::NTERM, name);
	empty->choices.emplace_back();
	empty->nullable = true;
	empty->defined = true;
	empty->predefined = true;
}

void check_undefined()
{
	bool err = false;
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		if (nterm->choices.empty()) {
			fprintf(stderr, "error: <%s> is undefined\n", nterm->name.c_str());
			err = true;
		}
	}
	if (err)
		exit(1);
}

void print_body(const vector<vector<Symbol*>> &choices);
void print_choice(const vector<Symbol*> &choice);

void print_symbol(Symbol *s)
{
	switch (s->kind) {
	case Symbol::TERM:
		fputs(s->name.c_str(), stdout);
		break;
	case Symbol::NTERM:
		{
			int opening = 0;
			if (s->choices.size() > 1) {
				if (s->choices.back().empty()) {
					size_t n = s->choices.size()-1;
					opening = '{';
					for (size_t i=0; i<n; i++) {
						if (s->choices[0].back() != s) {
							opening = '[';
							break;
						}
					}
				}
			}
			if (opening) {
				printf("%c ", opening);
				print_body(*s->choices_core);
				printf(" %c", closing_sym(opening));
			} else {
				printf("<%s>", s->name.c_str());
			}
		}
		break;
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
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		if (!nterm->predefined && !nterm->opening_sym()) {
			printf("<%s> ::= ", nterm->name.c_str());
			print_body(nterm->choices);
			putchar('\n');
		}
	}
}

void print_symbol_set(const char *set_name, const char *nterm_name, const set<Symbol*> s)
{
	printf("%s(<%s>) = {", set_name, nterm_name);
	for (Symbol *term: s)
		printf(" %s", term->name.c_str());
	puts(" }");
}

void print_first_follow()
{
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		if (!nterm->opening_sym()) {
			const char *nterm_name = nterm->name.c_str();
			print_symbol_set("FIRST", nterm_name, nterm->first);
			print_symbol_set("FOLLOW", nterm_name, nterm->follow);
		}
	}
}

void list_symbols()
{
	printf("terminals: %d\n", term_dict.size());
	for (auto &pair: term_dict)
		puts(pair.second->name.c_str());
	printf("nonterminals: %d\n", nterm_dict.size()-1); // -1 for <empty>
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		if (!nterm->predefined)
			puts(nterm->name.c_str());
	}
}

enum {
	PRINT_RULES,
	LIST_SYMBOLS,
	CHECK_GRAMMAR,
	PRINT_FIRST_FOLLOW,
} action;

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s {-c|-f|-l|-p} file\n", progname);
	exit(2);
}

int main(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "cflp")) != -1) {
		switch (opt) {
		case 'c':
			action = CHECK_GRAMMAR;
			break;
		case 'f':
			action = PRINT_FIRST_FOLLOW;
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
	default:
		assert(0);
	}
	return 0;
}
