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
extern int parser_errors;

int main(int argc, char **argv)
{
	if (argc != 2)
		usage();
	lexer_open(argv[1]);
	unique_ptr<Block> blk = parse();
	if (parser_errors)
		return 1;
	translate_all(move(blk));
	lexer_close();
	return 0;
}
