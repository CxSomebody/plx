#include <cstdio>
#include <cstdlib>
extern "C" {
#include "lexer.h"
}

void usage()
{
	fputs("usage: ...\n", stderr);
	exit(2);
}

void parse();

int main(int argc, char **argv)
{
	if (argc != 2)
		usage();
	lexer_open(argv[1]);
#if 0
	for (;;) {
		lex();
		if (!sym)
			break;
		printf("%.*s (%d)\n", token_len, token_start, sym);
	}
#else
	parse();
#endif
	lexer_close();
	return 0;
}
