#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "dynbitset.h"
#include "translate.h"
#include "dataflow.h"

using namespace std;

string vector_int_tostr(const vector<int> &v)
{
	stringstream ss;
	ss << '[';
	bool sep = false;
	for (int x: v) {
		if (sep)
			ss << ',';
		ss << x;
		sep = true;
	}
	ss << ']';
	return ss.str();
}

void TranslateEnv::insert_sync()
{
	int n = quads.size();
	map<string, int> labelmap;
	vector<vector<int>> pred(n), succ(n);
	vector<vector<int>> scalar_defsites(scalar_id);
	vector<dynbitset> live_out(n, dynbitset(scalar_id));
	vector<dynbitset> live_in (n, dynbitset(scalar_id));
	vector<dynbitset> use     (n, dynbitset(scalar_id));
	vector<int> gen(n, -1);
	vector<int> def(n, -1);
	vector<int> def_loc;
	vector<int> def_scalar;
	for (int i=0; i<n; i++) {
		const Quad &q(quads[i]);
		if (q.op == Quad::LABEL) {
			labelmap[static_cast<LabelOperand*>(q.c)->label] = i;
		} else {
			int a = compute_def_temp(q);
			if (a >= 0 && a < scalar_id) {
				int d = def_loc.size();
				def_loc.push_back(i);
				def_scalar.push_back(a);
				scalar_defsites[a].push_back(d);
				gen[i] = d;
				def[i] = a;
			}
			for_each_use(q, [&](int a) {
				if (a >= 0 && a < scalar_id)
					use[i].set(a);
			});
		}
	}
	vector<dynbitset> kill(n, dynbitset(def_loc.size()));
	for (int i=0; i<n; i++) {
		const Quad &q = quads[i];
		if (q.is_jump_or_branch()) {
			int dst = labelmap[static_cast<LabelOperand*>(q.c)->label];
			pred[dst].push_back(i);
			succ[i].push_back(dst);
		}
		if (!q.isjump() && i != n-1) {
			pred[i+1].push_back(i);
			succ[i].push_back(i+1);
		}
		int a = compute_def_temp(q);
		if (a >= 0 && a < scalar_id) {
			int g = gen[i];
			for (int d: scalar_defsites[a]) {
				if (d != g)
					kill[i].set(d);
			}
		}
	}
#if 0
	for (int i=0; i<n; i++) {
		fprintf(stderr, "%s\tgen=%d kill=%s\n",
			quads[i].tostr().c_str(),
			gen[i], kill[i].tostr().c_str());
	}
#endif
	vector<dynbitset> rd_out(n, dynbitset(def_loc.size()));
	vector<dynbitset> rd_in (n, dynbitset(def_loc.size()));
	bool changed;
	// reaching def
	do {
		changed = false;
		for (int i=0; i<n; i++) {
			dynbitset s = rd_in[i]-kill[i];
			if (gen[i] >= 0)
				s.set(gen[i]);
			if (rd_out[i].update(s))
				changed = true;
		}
		for (int i=0; i<n; i++) {
			for (int j: pred[i])
				rd_in[i] |= rd_out[j];
		}
	} while (changed);
	// live var
	do {
		changed = false;
		for (int i=n-1; i>=0; i--) {
			dynbitset s = live_out[i];
			if (def[i] >= 0)
				s.clear(def[i]);
			s |= use[i];
			if (live_in[i].update(s))
				changed = true;
		}
		for (int i=0; i<n; i++) {
			for (int j: succ[i])
				live_out[i] |= live_in[j];
		}
	} while (changed);
	dynbitset writeback_def(def_loc.size());
	auto sync_scalars = [this](const Quad &q) {
		Operand **args = q.args;
		dynbitset s(scalar_id);
		for (int j=0; args[j]; j++) {
			assert(args[j]->istemp());
			int id = astemp(args[j])->id;
			s.set(id);
		}
		return s;
	};
	for (int i=0; i<n; i++) {
		const Quad &q = quads[i];
		if (q.op == Quad::SYNCM) {
			dynbitset syncset = sync_scalars(q);
			rd_in[i].foreach([&](int d){
				if (syncset.get(def_scalar[d]))
					writeback_def.set(d);
			});
		}
	}
	dynbitset writeback_loc(n);
	writeback_def.foreach([&](int d){
		writeback_loc.set(def_loc[d]);
	});
#if 0
	fprintf(stderr, "insert writeback after: %s\n",
		vector_int_tostr(writeback_loc.to_vector()).c_str());
#endif

	vector<Quad> oldquads(move(quads));
	quads.clear();
	for (int i=0; i<n; i++) {
		const Quad &q = oldquads[i];
		if (q.op != Quad::SYNCM) {
			if (q.op == Quad::SYNCR) {
				dynbitset syncset = sync_scalars(q);
				live_out[i].foreach([&](int a){
					if (syncset.get(a))
						sync_mem(a);
				});
			} else {
				quads.push_back(q);
			}
		}
		if (writeback_loc.get(i)) {
			int a = compute_def_temp(q);
			assert(a >= 0 && a < scalar_id);
			sync_reg(a);
		}
	}
}

void TranslateEnv::optimize()
{
#if 0
	fprintf(stderr, "optimize: %s\n", procname.c_str());
#endif
	insert_sync();
	vector<unique_ptr<BB>> blocks = partition(quads);
	dump_cfg(procname, blocks);
	split_edges(blocks);
	dump_cfg(procname+"-split", blocks);
#if 1
	// compute dominator tree and dominance frontier
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
	//
	vector<dynbitset> defsites(tempid, dynbitset(n));
	for (const unique_ptr<BB> &p: blocks) {
		const BB *bb = p.get();
		dynbitset bdefs(tempid);
		for (const Quad &q: bb->quads) {
			int def = compute_def_temp(q);
			if (def >= 0)
				bdefs.set(def);
		}
		bdefs.foreach([&](int a) {
			defsites[a].set(bb->id);
		});
	}
#if 0
	for (int a=0; a<tempid; a++) {
		fprintf(stderr, "defsites[%d]=%s\n", a, defsites[a].tostr().c_str());
	}
#endif
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
					Operand **args = (Operand **) calloc(npred+1, sizeof *args);
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
	// rename variables
	function<void(int, vector<vector<int>>)> rename = [&](int b, vector<vector<int>> stack) {
		for (Quad &q: blocks[b]->quads) {
			// rename variables used in each non-phi quad
			if (q.op != Quad::PHI) {
				dynbitset use(tempid);
				for_each_use(q, [&](int a){
					if (a >= 0)
						use.set(a);
				});
				use.foreach([&](int a){
					int newa = stack[a].back();
					replace_use(q, a, newa);
				});
			}
			// rename variables defined in each quad
			int a = compute_def_temp(q);
			if (a >= 0) {
				int size = temps[a]->size;
				int newa = newtemp(size)->id;
				stack[a].push_back(newa);
				replace_def(q, a, newa);
			}
		}
		for (BB *succ: blocks[b]->succ) {
			int y = succ->id;
			// b is the j-th predecessor of y
			int j;
			int npred = succ->pred.size();
			for (j=0; j < npred && succ->pred[j]->id != b; j++);
			assert(j < npred);
			// for each phi function in y
			vector<Quad> &quads(blocks[y]->quads);
			for (auto it = quads.begin(); it != quads.end() && it->op == Quad::PHI; it++) {
				Quad &q = *it;
				Operand *arg = q.args[j];
				assert(arg->istemp());
				TempOperand *t = astemp(arg);
				q.args[j] = new TempOperand(t->size, stack[t->id].back());
			}
		}
		children[b].foreach([&](int child){
			rename(child, stack);
		});
	};
	vector<vector<int>> stack(tempid);
	for (int a=0; a<tempid; a++)
		stack[a].push_back(a);
	rename(0, stack);
	dump_cfg(procname+"-ssa", blocks);
#endif
}
