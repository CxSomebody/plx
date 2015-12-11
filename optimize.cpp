#include <cassert>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"
#include "dynbitset.h"

using namespace std;

vector<dynbitset> domfront(const vector<unique_ptr<BB>> &blocks);

void TranslateEnv::optimize()
{
	fprintf(stderr, "optimize: %s\n", procname.c_str());
	vector<unique_ptr<BB>> blocks = partition(quads);
	char fname[80];
	sprintf(fname, "cfg-%s.dot", procname.c_str());
	blocks_to_dot(blocks, fname);
	domfront(blocks);
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
		fprintf(fp, "\tB%d [shape=box label=\"Block %d:\\n%s\"];\n", bb->id, bb->id, ss.str().c_str());
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

vector<dynbitset> domfront(const vector<unique_ptr<BB>> &blocks)
{
	int n = blocks.size();
	vector<dynbitset> sdom(n, dynbitset(n));
	vector<int> idom(n, -1);
	{
		vector<dynbitset> &dom(sdom); // just an alias
		// dominator of the start node is itself
		dom[0].set(0);
		// for all other nodes, set all nodes as the dominators
		for (int i=1; i<n; i++)
			dom[i].set_all();
		// iteratively eliminate nodes that are not dominators
		bool changed;
		do {
			changed = false;
			for (int i=1; i<n; i++) {
				dynbitset s(n);
				s.set_all();
				for (const BB *p: blocks[i]->pred)
					s &= dom[p->id];
				s.set(i);
				if (dom[i].update(s))
					changed = true;
			}
		} while (changed);
#if 0
		for (int i=0; i<n; i++) {
			fprintf(stderr, "dom[%d]=%s\n", i, dom[i].tostr().c_str());
		}
#endif
	}
	for (int i=0; i<n; i++)
		sdom[i].clear(i);
	for (int i=1; i<n; i++) {
		dynbitset s(n);
		// union sdom[j] for j in sdom[i]
		sdom[i].foreach([&](int j){ s |= sdom[j]; });
		// should contain exactly 1 element, that is the idom
		s = sdom[i]-s;
		s.foreach([&](int j) {
			assert(idom[i] < 0);
			idom[i] = j;
		});
#if 1
		fprintf(stderr, "idom[%d]=%d\n", i, idom[i]);
#endif
	}
	vector<dynbitset> domfront(n, dynbitset(n));
	return domfront;
}
