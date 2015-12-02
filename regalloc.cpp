#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include "translate.h"
#include "dataflow.h"

using namespace std;

vector<int> color_graph(Graph &&g)
{
	constexpr int k=6; // eax ecx edx; ebx esi edi
	int ntemp = g.ntemp();
	vector<int> color(ntemp, -1);
	vector<int> removed;
	removed.reserve(ntemp);
	bool removed_p[ntemp] = {false};
	vector<vector<int>> neighbors(ntemp);
	auto remove_node = [&](int i) {
		assert(!removed_p[i]);
		neighbors[i] = g.remove(i);
		removed.push_back(i);
		removed_p[i] = true;
	};
	function<void()> remove_nodes = [&]() {
		for (int i=0; i<ntemp; i++) {
			int n = g.neighbors(i).size();
			if (!removed_p[i] && n<k) {
				remove_node(i);
				return remove_nodes();
			}
		}
		for (int i=0; i<ntemp; i++) {
			if (!removed_p[i]) {
				remove_node(i);
				return remove_nodes();
			}
		}
	};
	remove_nodes();
	assert(removed.size() == size_t(ntemp));
	while (!removed.empty()) {
		int t = removed.back();
		removed.pop_back();
		unsigned char f = 0x30; // 8 bits for 8 regs
		for (int a: neighbors[t]) {
			if (a<0)
				f |= 1<<~a;
			else if (color[a] >= 0)
				f |= 1<<color[a];
		}
		for (int i=0; i<8; i++) {
			if (!(f&(1<<i))) {
				color[t] = i;
				break;
			}
		}
		printf("color[%d] = %d\n", t, color[t]);
	}
	return color;
};
