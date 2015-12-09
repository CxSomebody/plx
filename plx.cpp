#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

extern "C" {
#include "lexer.h"
#include "tokens.h"
}
#include "semant.h"
#include "translate.h"

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
	int opt;
	TranslateOptions options;
	while ((opt = getopt(argc, argv, "O")) != -1) {
		switch (opt) {
		case 'O':
			options.opt = 1;
			break;
		default:
			usage();
		}
	}
	if (optind != argc-1)
		usage();
	lexer_open(argv[optind]);
	unique_ptr<Block> blk = parse();
	if (parser_errors)
		return 1;
	translate_all(move(blk), options);
	lexer_close();
	return 0;
}
