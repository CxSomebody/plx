#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"

using namespace std;

vector<unique_ptr<BB>> partition(const vector<Quad> &quads)
{
	map<string, BB*> table;
	vector<unique_ptr<BB>> blocks;
	int id = 0;
	auto begin = quads.begin();
	auto end = begin;
	for (;;) {
		while (end != quads.end() && end->op == Quad::LABEL)
			end++;
		while (end != quads.end() && end->op != Quad::LABEL && !end->is_jump_or_branch())
			end++;
		if (end != quads.end() && end->is_jump_or_branch())
			end++;
		if (end == begin)
			break;
		BB bb;
		bb.id = id++;
		bb.quads.insert(bb.quads.end(), begin, end);
		blocks.push_back(make_unique<BB>(move(bb)));
		begin = end;
	}
	// set up lookup table
	for (const unique_ptr<BB> &bb: blocks) {
		for (auto it = bb->quads.begin();
		     it != bb->quads.end() && it->op == Quad::LABEL;
		     it++)
		{
			const string &label = static_cast<LabelOperand*>(it->c)->label;
			table[label] = bb.get();
		}
	}
	// compute pred & succ
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		const unique_ptr<BB> &bb = *it;
		assert(!bb->quads.empty());
		const Quad &lastq = bb->quads.back();
		if (lastq.is_jump_or_branch()) {
			BB *t = table[static_cast<LabelOperand*>(lastq.c)->label];
			bb->succ.push_back(t);
			t->pred.push_back(bb.get());
		}
		if (!lastq.isjump() && next(it) != blocks.end()) {
			BB *f = next(it)->get();
			bb->succ.push_back(f);
			f->pred.push_back(bb.get());
		}
	}
	return blocks;
}

constexpr size_t log2_r(size_t n, size_t a)
{
        return n == 1 ? a : log2_r(n>>1, a+1);
}
constexpr size_t log2(size_t n)
{
        return log2_r(n, 0);
}

class dynbitset {
	typedef unsigned long word;
	static const size_t wsize = sizeof(word)*8;
	static const size_t lwsize = log2(wsize);
	vector<word> data;
public:
	dynbitset(size_t size): data((size+wsize-1)>>lwsize) {}
	bool get(int index) const
	{
		return data[index>>lwsize] & 1<<((index&lwsize)-1);
	}
	void set(int index)
	{
		data[index>>lwsize] |= 1<<((index&lwsize)-1);
	}
	dynbitset operator|(const dynbitset &that)
	{
		dynbitset result(*this);
		for (size_t i=0; i<data.size(); i++)
			result.data[i] |= that.data[i];
		return result;
	}
	dynbitset operator-(const dynbitset &that)
	{
		dynbitset result(*this);
		for (size_t i=0; i<data.size(); i++)
			result.data[i] &= ~that.data[i];
		return result;
	}
};

#if 0
vector<vector<int>> livevar(const vector<unique_ptr<BB>> &blocks, int n)
{
}
#endif
