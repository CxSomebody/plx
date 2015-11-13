#include <cassert>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include "tokens.h"

extern char *yytext;
extern int yyleng;
extern int yylineno;
extern char tokval;

extern "C" int yylex();

int sym;

// symbol information

struct Symbol;

int char_term_id[256];
std::map<std::string, Symbol*> term_dict, nterm_dict;

struct Symbol {
	enum SymbolKind { TERM, NTERM } kind;
	std::string name;
	union {
		struct {
			std::vector<std::vector<Symbol*>> *choices;
			bool defined;
		};
	};
	int opening_sym() {
		char opening = name[0];
		switch (opening) {
		case '(': case '[': case '{':
			return opening;
		}
		return 0;
	}
	Symbol(SymbolKind k, const std::string &name): name(name) {
		switch (kind = k) {
		case TERM:
			break;
		case NTERM:
			choices = new std::vector<std::vector<Symbol*>>();
			defined = false;
			break;
		default:
			assert(0);
		}
	}
	~Symbol() {
		switch (kind) {
		case TERM:
			break;
		case NTERM:
			delete choices;
			break;
		default:
			assert(0);
		}
	}
	Symbol(const Symbol &) = delete;
};

// parser routines

void getsym()
{
	sym = yylex();
#if 0
	switch (sym) {
	case IS:
		puts("::=");
		break;
	case CHAR:
		printf("CHAR %c\n", tokval);
		break;
	case IDENT:
		printf("IDENT %s\n", yytext);
		break;
	case NTERM:
		printf("IDENT %.*s\n", yyleng-2, yytext+1);
		break;
	case '\n':
		puts("NEWLINE");
		break;
	default:
		printf("%c\n", sym);
	}
#endif
}

void error()
{
	fprintf(stderr, "%d: syntax error (sym=%d)\n", yylineno, sym);
	exit(1);
}

void parse_body(std::vector<std::vector<Symbol*>> &body);

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

void parse_seq(std::vector<Symbol *> &choice)
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
				std::string nterm_name(synth_name);
				Symbol *nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				getsym();
				parse_body(*nterm->choices);
				if (sym == closing_sym(opening)) getsym();
				else error();
				choice.emplace_back(nterm);
			}
			break;
		case CHAR:
			{
				char synth_name[4];
				sprintf(synth_name, "'%c'", tokval);
				std::string term_name(synth_name);
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
				std::string nterm_name(yytext+1, yytext+yyleng-1);
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
				std::string term_name(yytext);
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

void parse_body(std::vector<std::vector<Symbol*>> &body)
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
	std::string nterm_name;
	if (sym == NTERM) {
		nterm_name = std::string(yytext+1, yytext+yyleng-1);
		getsym();
	} else {
		error();
	}
	if (sym == IS) getsym();
	else error();

	Symbol *nterm = nterm_dict[nterm_name];
	if (nterm) {
		if (nterm->defined) {
			fprintf(stderr, "%d: redefinition of <%s>\n",
				yylineno, nterm_name.c_str());
			exit(1);
		} else {
			nterm->defined = true;
		}
	} else {
		nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
	}

	parse_body(*nterm->choices);
	if (sym == '\n') getsym();
	else error();
}

void print_body(const std::vector<std::vector<Symbol*>> &choices);

void print_symbol(Symbol *s)
{
	switch (s->kind) {
	case Symbol::TERM:
		fputs(s->name.c_str(), stdout);
		break;
	case Symbol::NTERM:
		{
			char opening = s->opening_sym();
			if (opening) {
				char closing = closing_sym(opening);
				printf("%c ", opening);
				print_body(*s->choices);
				printf(" %c", closing);
			} else {
				printf("<%s>", s->name.c_str());
			}
		}
		break;
	}
}

void print_choice(const std::vector<Symbol*> &choice)
{
	auto n = choice.size();
	for (decltype(n) i=0; i<n; i++) {
		print_symbol(choice[i]);
		if (i < n-1)
			putchar(' ');
	}
}

void print_body(const std::vector<std::vector<Symbol*>> &choices)
{
	auto n = choices.size();
	for (decltype(n) i=0; i<n; i++) {
		print_choice(choices[i]);
		if (i < n-1)
			fputs(" | ", stdout);
	}
}

void print_rules()
{
	for (auto &pair: nterm_dict) {
		Symbol *nterm = pair.second;
		if (!nterm->opening_sym()) {
			printf("<%s> ::= ", nterm->name.c_str());
			print_body(*nterm->choices);
			putchar('\n');
		}
	}
}

int main()
{
	for (int i=0; i<256; i++)
		char_term_id[i] = -1;
	getsym();
	while (sym)
		parse_rule();
#if 0
	printf("terminals: %d\n", term_dict.size());
	for (auto &pair: term_dict)
		puts(pair.second->name.c_str());
	printf("nonterminals: %d\n", nterm_dict.size());
	for (auto &pair: nterm_dict)
		puts(pair.second->name.c_str());
#endif
	print_rules();
	return 0;
}
