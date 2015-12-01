struct BB {
	std::vector<Quad> quads;
	std::vector<BB*> pred, succ;
	int id;
};

class Graph
{
	std::vector<std::vector<int>> neighbors;
	int end;
public:
	Graph(int end): neighbors(8+end), end(end) {}
	void connect(int a, int b);
	void print() const;
	int degree(int v) const
	{
		return neighbors[8+v].size();
	}
};

std::vector<std::unique_ptr<BB>> partition(const std::vector<Quad> &quads);
Graph local_livevar(const BB &bb, size_t ntemp);
