#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "lexer.h"
#include "tokens.h"
}
#include "semant.h"

using namespace std;

void usage()
{
	fputs("usage: ...\n", stderr);
	exit(2);
}

unique_ptr<Block> parse();
extern int syntax_errors;

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
	unique_ptr<Block> blk = parse();
	if (!syntax_errors)
		translate(move(blk));
#endif
	lexer_close();
	return 0;
}
