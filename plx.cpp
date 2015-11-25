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

void parse();
extern int syntax_errors;

int main(int argc, char **argv)
{
	if (argc != 2)
		usage();
	lexer_open(argv[1]);
	parse();
	lexer_close();
	return 0;
}
