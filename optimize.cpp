#include <cassert>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "translate.h"
#include "dynbitset.h"
#include "dataflow.h"

using namespace std;

vector<dynbitset> compute_domfront(const vector<unique_ptr<BB>> &blocks);

void TranslateEnv::optimize()
{
	fprintf(stderr, "optimize: %s\n", procname.c_str());
	vector<unique_ptr<BB>> blocks = partition(quads);
	int n = blocks.size();
	vector<dynbitset> domfront = compute_domfront(blocks);
	vector<dynbitset> defsites(tempid, dynbitset(n));
	for (const unique_ptr<BB> &p: blocks) {
		const BB *bb = p.get();
		dynbitset bdefs(tempid);
		for (const Quad &q: bb->quads) {
			compute_def(q, bdefs, true);
		}
		bdefs.foreach([&](int a) {
			defsites[a].set(bb->id);
		});
	}
	for (int a=0; a<tempid; a++) {
		fprintf(stderr, "defsites[%d]=%s\n", a, defsites[a].tostr().c_str());
	}
	// place phi functions
	// for each scalar a
	for (int a=0; a<tempid; a++) {
		dynbitset f(n); // set of blocks where phi is added
		dynbitset w(defsites[a]);
		while (!w.empty()) {
			int x = w.first();
			w.clear(x);
			domfront[x].foreach([&](int y) {
				if (!f.get(y)) {
					BB &block_y(*blocks[y]);
					// insert phi at block y
					int npred = block_y.pred.size();
					Operand **args = (Operand **) calloc(sizeof *args, npred+1);
#if 1
					for (int i=0; i<npred; i++)
						args[i] = temps[a];
#endif
					block_y.quads.emplace(block_y.quads.begin(), Quad::PHI, temps[a], args);
					f.set(y);
					if (!defsites[a].get(y))
						w.set(y);
				}
			});
		}
	}
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

vector<dynbitset> compute_domfront(const vector<unique_ptr<BB>> &blocks)
{
	int n = blocks.size();
	vector<dynbitset> dom(n, dynbitset(n));
	vector<int> idom(n, -1);
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
	vector<dynbitset> sdom(dom);
	for (int i=0; i<n; i++)
		sdom[i].clear(i);
	vector<dynbitset> children(n, dynbitset(n));
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
#if 0
		fprintf(stderr, "idom[%d]=%d\n", i, idom[i]);
#endif
		children[idom[i]].set(i);
	}
	vector<dynbitset> domfront(n, dynbitset(n));
	// DF[i] = DF_local[i] | |DF_up[c] for c in children[i]
	function<void(int)> compute_df = [&](int i) {
		dynbitset s(n);
		// this loop computes DF_local[n]
		for (const BB *succ: blocks[i]->succ) {
			int y = succ->id;
			if (idom[y] != i)
				s.set(y);
		}
		children[i].foreach([&](int c) {
			compute_df(c);
			// this loop computes DF_up[c]
			domfront[c].foreach([&](int w) {
				if (!dom[w].get(i)) // i not in dom[w]; that is i does not dominate w
					s.set(w);
			});
		});
		domfront[i] = s;
	};
	for (int i=0; i<n; i++) {
		compute_df(i);
#if 0
		fprintf(stderr, "DF[%d]=%s\n", i, domfront[i].tostr().c_str());
#endif
	}

	return domfront;
}
