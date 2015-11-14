#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct Symbol {
	enum SymbolKind { TERM, NTERM } kind;
	std::string name;
	std::vector<std::vector<Symbol*>> choices;
	std::unique_ptr<std::vector<std::vector<Symbol*>>> choices_core;
	std::set<Symbol*> first, follow;
	bool nullable = false;
	bool defined = false;
	int opening_sym();
	Symbol(SymbolKind kind, const std::string &name);
};

extern std::map<std::string, Symbol*> term_dict, nterm_dict;
extern Symbol *top;
extern Symbol *empty;

template <typename F>
void for_each_term(F f) {
	for (auto &pair: term_dict)
		f(pair.second);
}

template <typename F>
void for_each_nterm(F f) {
	for (auto &pair: nterm_dict)
		f(pair.second);
}

// reachable nonterminals only
struct NTermVisitor {
	std::set<Symbol*> vis; // set of visited nonterminals
	std::function<void(Symbol*)> f;
	void visit(Symbol *nterm);
	NTermVisitor(std::function<void(Symbol*)> f);
};
