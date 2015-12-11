#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"

using namespace std;

void TranslateEnv::optimize()
{
	vector<unique_ptr<BB>> blocks = partition(quads);
	char fname[80];
	sprintf(fname, "cfg-%s.dot", procname.c_str());
	blocks_to_dot(blocks, fname);
}

bool blocks_to_dot(const vector<unique_ptr<BB>> &blocks, const char *fpath)
{
	FILE *fp = fopen(fpath, "w");
	if (!fp) {
		perror(fpath);
		return false;
	}
	fprintf(fp, "digraph {\n");
	// declare all basic blocks
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		const BB *bb = it->get();
		stringstream ss;
		for (const Quad &q: bb->quads) {
			ss << q.tostr() << "\\n";
		}
		fprintf(fp, "\tB%d [shape=box label=\"%s\"];\n", bb->id, ss.str().c_str());
	}
	// arcs
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		const BB *bb = it->get();
		for (const BB *succ: bb->succ)
			fprintf(fp, "\tB%d -> B%d;\n", bb->id, succ->id);
	}
	fprintf(fp, "}\n");
	fclose(fp);
	return true;
}
