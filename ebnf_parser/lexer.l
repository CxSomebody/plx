%x XQUOTE
%x XINLINE
%option noyywrap nodefault yylineno
ID	[a-zA-Z_][a-zA-Z_0-9]*
%{
#include <assert.h>
#include "tokens.h"
int tokval;
int lexenv;
static int qlevel;
#define MAX_QUOTE 255
char qtext[MAX_QUOTE+1];
int qlen;
%}
%%
<XQUOTE>{
	\<	{ assert(qlen < MAX_QUOTE); qtext[qlen++] = '<'; qlevel++; }
	\>	{ if (!--qlevel) { qtext[qlen] = 0; BEGIN(INITIAL); return QUOTE; } assert(qlen < MAX_QUOTE); qtext[qlen++] = '>'; }
	[^<>]*	{ assert(qlen+yyleng <= MAX_QUOTE); memcpy(qtext+qlen, yytext, yyleng); qlen += yyleng; }
}
<XINLINE>{
	\{	{ assert(qlen < MAX_QUOTE); qtext[qlen++] = '{'; qlevel++; }
	\}	{ if (!--qlevel) { qtext[qlen] = 0; BEGIN(INITIAL); return ACTION_INLINE; } assert(qlen < MAX_QUOTE); qtext[qlen++] = '}'; }
	[^{}]*	{ assert(qlen+yyleng <= MAX_QUOTE); memcpy(qtext+qlen, yytext, yyleng); qlen += yyleng; }
}
::=	return IS;
::	return AS;
'.'	{ tokval = ((unsigned char *)yytext)[1]; return CHAR; }
{ID}	return IDENT;
@{ID}	return ACTION_NAMED;
[ \t]+
\<	{ BEGIN(XQUOTE); qlen = 0; qlevel = 1; }
@\{	{ BEGIN(XINLINE);qlen = 0; qlevel = 1; }
\n	return '\n';
.	{
		if (strchr("<>|()[]{}@?", *yytext) ||
		    lexenv == 1 && *yytext == ',')
			return *yytext;
		tokval = *yytext;
		return CHAR;
	}
