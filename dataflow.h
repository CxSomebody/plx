struct BB {
	std::vector<Quad> quads;
	std::vector<BB*> pred, succ;
	int id;
	BB(int id): id(id) {}
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
std::vector<int> color_graph(Graph &&g);
bool blocks_to_dot(const std::vector<std::unique_ptr<BB>> &blocks,
		   const char *fpath);
void compute_def(const Quad &q, dynbitset &ret);
void for_each_use(const Quad &q, std::function<void(int)> f);
int compute_def_temp(const Quad &q);
void replace_def(Quad &q, int old, int neu);
void replace_use(Quad &q, int old, int neu);
void split_edges(std::vector<std::unique_ptr<BB>> &blocks);
void dump_cfg(const std::string &procname, const std::vector<std::unique_ptr<BB>> &blocks);
