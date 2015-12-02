struct BB {
	std::vector<Quad> quads;
	std::vector<BB*> pred, succ;
	int id;
};

class Graph
{
	std::vector<std::vector<int>> _neighbors;
	int end;
public:
	Graph(int end): _neighbors(8+end), end(end) {}
	std::vector<int> &neighbors(int v)
	{
		return _neighbors[8+v];
	}
	const std::vector<int> &neighbors(int v) const
	{
		return _neighbors[8+v];
	}
	int ntemp() const { return end; }
	void connect(int a, int b);
	std::vector<int> remove(int v);
	void print() const;
	int degree(int v) const
	{
		return neighbors(v).size();
	}
};

std::vector<std::unique_ptr<BB>> partition(const std::vector<Quad> &quads);
void local_livevar(const BB &bb, size_t ntemp, Graph &ig);
std::vector<int> color_graph(Graph &&g);
