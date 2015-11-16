#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

void usage(char *progname)
{
	fprintf(stderr, "usage: %s <file>\n", progname);
	exit(2);
}

int main(int argc, char **argv)
{
	if (argc != 2)
		usage(argv[0]);
	lexer_open(argv[1]);
	for (;;) {
		getsym();
		if (!sym)
			break;
		if (sym < 256)
			printf("%c\n", sym);
		else
			printf("%s [%.*s]\n", tokname[sym-256],
			       toklen, tokstart);
	}
	lexer_close();
	return 0;
}
