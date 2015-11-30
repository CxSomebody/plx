struct BB {
	std::vector<Quad> quads;
	std::vector<BB*> pred, succ;
	int id;
};

std::vector<std::unique_ptr<BB>> partition(const std::vector<Quad> &quads);
