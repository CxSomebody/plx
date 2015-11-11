#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

void usage()
{
	fputs("usage: ...\n", stderr);
	exit(2);
}

int main(int argc, char **argv)
{
	if (argc != 2)
		usage();
	lexer_open(argv[1]);
	for (;;) {
		lex();
		if (!sym)
			break;
		printf("%.*s (%d)\n", token_len, token_start, sym);
	}
	lexer_close();
	return 0;
}
