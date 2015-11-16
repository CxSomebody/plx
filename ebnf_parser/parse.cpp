#include <cassert>
#include <cstdio>
#include <cstring>
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

int closing_sym(int opening);

static void parse_body(Symbol *lhs);

static void synth_nterm_name(int kind, char *name)
{
	static int id;
	char c;
	switch (kind) {
	case '(': c = 's'; break;
	case '[': c = 'o'; break;
	case '{': c = 'm'; break;
	default: assert(0);
	}
	sprintf(name, "_%c%d", c, id);
	id++;
}

static void parse_seq(vector<Symbol *> &choice, Symbol *lhs, int choice_id)
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
				parse_body(nterm);
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
				getsym();
				if (lhs && sym == AS) {
					getsym();
					// expect IDENT: name of semantic predicate function
					if (sym == IDENT) {
						string guarded_term_name(term_name + "::" + yytext);
						Symbol *guarded_term = term_dict[guarded_term_name];
						if (!guarded_term) {
							guarded_term = term_dict[guarded_term_name] =
								new Symbol(Symbol::TERM, term_name);
						}
						guarded_term->sp = strdup(yytext);
						term = guarded_term;
						getsym();
					} else {
						syntax_error();
					}
				}
				choice.emplace_back(term);

			}
			break;
		default:
			if (choice.size() == 1 && choice[0] == empty)
				choice.clear();
			return;
		}
	}
}

static void parse_body(Symbol *lhs)
{
	vector<vector<Symbol*>> &body = lhs->choices;
	for (;;) {
		int choice_id = body.size();
		body.emplace_back();
		parse_seq(body.back(), lhs, choice_id);
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

	if (nterm_name == "WEAK") {
		vector<Symbol*> weak_symbols;
		parse_seq(weak_symbols, nullptr, 0);
		for (Symbol *s: weak_symbols)
			s->weak = true;
	} else {
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

		parse_body(nterm);
	}

	if (sym == '\n') getsym();
	else syntax_error();
}

void parse()
{
	getsym();
	while (sym)
		parse_rule();
}
