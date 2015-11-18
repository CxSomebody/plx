#include <cassert>
#include <cstdio>
#include <cstring>
#include "ebnf.h"
#include "tokens.h"

using namespace std;

extern char *yytext;
extern int yyleng;
extern int yylineno;
extern int tokval;
extern int lexenv;
extern char qtext[];
extern int qlen;

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

// (a, b, c, ...)
static vector<string> parse_arg_list(void)
{
	lexenv = 1;
	vector<string> list;
	if (sym == '(') getsym();
	else syntax_error();
	if (sym == ')') {
		getsym();
		goto finish;
	}
	for (;;) {
		if (sym != IDENT) syntax_error();
		list.emplace_back(yytext);
		getsym();
		if (sym == ')') {
			getsym();
			goto finish;
		}
		if (sym == ',') getsym();
		else syntax_error();
	}
finish:
	lexenv = 0;
	return list;
}

static Param parse_param()
{
	if (sym != QUOTE) syntax_error();
	string type(qtext, qlen);
	getsym();
	string name;
	if (sym == IDENT) {
		name = yytext;
		getsym();
	}
	return Param(type, name);
}

// (<A> a, <B> b, <C> c, ...)
static vector<Param> parse_param_list()
{
	lexenv = 1;
	vector<Param> list;
	if (sym == '(') getsym();
	else syntax_error();
	if (sym == ')') {
		getsym();
		goto finish;
	}
	for (;;) {
		list.emplace_back(parse_param());
		if (sym == ')') {
			getsym();
			goto finish;
		}
		if (sym == ',') getsym();
		else syntax_error();
	}
finish:
	lexenv = 0;
	return list;
}

static ArgList parse_args()
{
	ArgList args;
	if (sym == '(') {
		args.out = parse_arg_list();
	} else if (sym == IDENT) {
		args.out.emplace_back(yytext);
		getsym();
	} else {
		syntax_error();
	}
	if (sym == '(') {
		args.in = parse_arg_list();
	}
	return args;
}

static ParamList parse_params()
{
	ParamList params;
	if (sym == '(') {
		params.out = parse_param_list();
	} else if (sym == QUOTE) {
		params.out.emplace_back(parse_param());
	} else {
		syntax_error();
	}
	if (sym == '(') {
		params.in = parse_param_list();
	}
	return params;
}

static void parse_choice(Choice &choice, Symbol *lhs, int choice_id)
{
	for (;;) {
		Symbol *newsym;
		switch (sym) {
		case '(':
		case '[':
		case '{':
			{
				char synth_name[16];
				int opening = sym;
				synth_nterm_name(sym, synth_name);
				string nterm_name(synth_name);
				newsym = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				getsym();
				parse_body(newsym);
				if (sym == closing_sym(opening)) getsym();
				else syntax_error();
				newsym->choices_core = make_unique<vector<Choice>>(newsym->choices);
				switch (opening) {
				case '{':
					for (auto &c: newsym->choices)
						c.emplace_back(new Instance(newsym));
				       	newsym->choices.emplace_back();
					break;
				case '[':
				       	newsym->choices.emplace_back();
					break;
				case '(':
					break;
				default:
					assert(0);
				}
			}
			break;
		case CHAR:
			{
				char synth_name[4];
				sprintf(synth_name, "'%c'", tokval);
				string term_name(synth_name);
				newsym = term_dict[term_name];
				if (!newsym) {
					newsym = term_dict[term_name] = new Symbol(Symbol::TERM, term_name);
				}
				getsym();
			}
			break;
		case QUOTE:
			{
				string nterm_name(qtext, qlen);
				Symbol *nterm = nterm_dict[nterm_name];
				if (!nterm) {
					nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				}
				newsym = nterm;
				getsym();
			}
			break;
		case IDENT:
			{
				string term_name(yytext);
				newsym = term_dict[term_name];
				if (!newsym) {
					newsym = term_dict[term_name] = new Symbol(Symbol::TERM, term_name);
				}
				getsym();
				if (lhs && sym == '?') {
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
						newsym = guarded_term;
						getsym();
					} else {
						syntax_error();
					}
				}
			}
			break;
		case ACTION_NAMED:
			{
				string name(yytext+1); // +1 for leading '@'
				getsym();
				newsym = action_dict[name];
				if (!newsym)
					newsym = action_dict[name] = new Symbol(Symbol::ACTION, name);
				newsym->nullable = true;
			}
			break;
		case ACTION_INLINE:
			{
				newsym = new Symbol(Symbol::ACTION, "");
				newsym->action = strdup(qtext);
				getsym();
				newsym->nullable = true;
			}
			break;
		default:
			// S ::= ... | <empty> | ...
			if (choice.size() == 1 && choice[0]->sym == empty)
				choice.clear();
			return;
		}
		// ::out
		// ::(out...)
		Instance *newinst;
		if (sym == AS) {
			getsym();
			newinst = new Instance(newsym, parse_args());
		} else {
			newinst = new Instance(newsym);
		}
		choice.emplace_back(newinst);
	}
}

static void parse_body(Symbol *lhs)
{
	vector<Choice> &body = lhs->choices;
	for (;;) {
		int choice_id = body.size();
		body.emplace_back();
		parse_choice(body.back(), lhs, choice_id);
		if (sym != '|')
			break;
		getsym();
	}
}

static void parse_rule()
{
	string nterm_name;
	if (sym != QUOTE) syntax_error();
	nterm_name = string(qtext, qlen);
	getsym();

#ifdef ENABLE_WEAK
	// TODO fix broken code
	if (nterm_name == "WEAK") {
		Choice weak_symbols;
		parse_choice(weak_symbols, nullptr, 0);
		for (Symbol *s: weak_symbols)
			s->weak = true;
	} else {
#endif
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
#ifdef ENABLE_WEAK
	}
#endif

	if (sym == AS) {
		fprintf(stderr, "nterm with params: %s\n", nterm->name.c_str());
		getsym();
		nterm->params = parse_params();
	}

	if (sym != IS) syntax_error();
	getsym();

	parse_body(nterm);

	if (sym != '\n') syntax_error();
	getsym();
}

static void parse_decl()
{
	Symbol *s;
	if (sym == IDENT) {
		string name(yytext);
		getsym();
		s = term_dict[name];
		if (!s)
			s = term_dict[name] = new Symbol(Symbol::TERM, name);
	} else if (sym == ACTION_NAMED) {
		string name(yytext+1);
		getsym();
		s = action_dict[name];
		if (!s)
			s = action_dict[name] = new Symbol(Symbol::ACTION, name);
	} else {
		syntax_error();
	}
	if (sym != AS) syntax_error();
	getsym();
	s->params = parse_params();
	if (sym != '\n') syntax_error();
	getsym();
}

void parse()
{
	getsym();
	while (sym) {
		if (sym == QUOTE) parse_rule();
		else parse_decl();
	}
}
