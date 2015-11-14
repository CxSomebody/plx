#include <cassert>
#include <cstdio>
#include "ebnf.h"
#include "tokens.h"

using namespace std;

extern char *yytext;
extern int yyleng;
extern int yylineno;
extern char tokval;

extern "C" int yylex();

static int sym;

static void getsym()
{
	sym = yylex();
}

static void syntax_error()
{
	fprintf(stderr, "%d: error: syntax error (sym=%d)\n", yylineno, sym);
	exit(1);
}

static void parse_body(vector<vector<Symbol*>> &body);

int closing_sym(int opening);

static void synth_nterm_name(int kind, char *name)
{
	static int id;
	sprintf(name, "%c%d%c", kind, id, closing_sym(kind));
	id++;
}

static void parse_seq(vector<Symbol *> &choice)
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
				nterm->choices_core = make_unique<vector<vector<Symbol*>>>(nterm->choices);
				switch (opening) {
				case '{':
					for (auto &choice: nterm->choices)
						choice.emplace_back(nterm);
				       	nterm->choices.emplace_back();
					break;
				case '[':
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

static void parse_body(vector<vector<Symbol*>> &body)
{
	for (;;) {
		body.emplace_back();
		parse_seq(body.back());
		if (sym != '|')
			break;
		getsym();
	}
}

static void parse_rule()
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
