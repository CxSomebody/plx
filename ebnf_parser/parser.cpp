#include <cstdlib>
#include <string>
#include "tokens.h"

extern char tokval;
extern int yylineno;

int sym;

void getsym()
{
	sym = yylex();
}

void error()
{
	fprintf(stderr, "%d: syntax error\n", yylineno);
	exit(1);
}

void parse_seq()
{
	switch (sym) {
	case '(':
		getsym();
		parse_body();
		if (sym == ')') getsym();
		else error();
		break;
	case '[':
		getsym();
		parse_body();
		if (sym == ']') getsym();
		else error();
		break;
	case '{':
		getsym();
		parse_body();
		if (sym == '}') getsym();
		else error();
		break;
	default:
		while (sym == CHAR || sym == IDENT || sym == NTERM)
			getsym();
	}
}

void parse_body()
{
	for (;;) {
		parse_seq();
		if (sym != '|')
			break;
		getsym();
	}
}

void parse_rule()
{
	std::string nterm_name;
	if (sym == NTERM) {
		nterm_name = std::string
			(std::begin(yytext)+1, std::end(yytext)-1);
	} else {
		error();
	}
	expect(NTERM);
	expect(IS);
	parse_body();
	printf("<%s> parsed\n", nterm_name.c_str());
}

int main()
{
	getsym();
	while (sym)
		parse_rule();
	return 0;
}
