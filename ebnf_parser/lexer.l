%{
#include "tokens.h"
int tokval;
%}
%option noyywrap nodefault yylineno
ID	[a-zA-Z_][a-zA-Z_0-9]*
NTERM	\<{ID}\>
%%
::=	return IS;
'.'	{ tokval = ((unsigned char *)yytext)[1]; return CHAR; }
{ID}	return IDENT;
{NTERM}	return NTERM;
[ \t]+
"//".*
\n	return '\n';
.	{
		if (strchr("|()[]{}", *yytext))
			return *yytext;
		tokval = *yytext;
		return CHAR;
	}
