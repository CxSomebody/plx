struct BB {
	std::vector<Quad> quads;
	std::vector<BB*> pred, succ;
	int id;
};

class Graph
{
	std::vector<std::vector<int>> neighbors;
	size_t _size;
public:
	Graph(size_t size): neighbors(size), _size(size) {}
	void connect(int a, int b);
	void print() const;
};

std::vector<std::unique_ptr<BB>> partition(const std::vector<Quad> &quads);
Graph local_livevar(const BB &bb, size_t ntemp);
