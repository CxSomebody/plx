#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct ArgList;
struct Instance;

// TODO unique_ptr<Instance> would be better
typedef std::vector<Instance*> Choice;

struct Param {
	std::string type;
	std::string name;
	Param(const std::string &type, const std::string &name);
};

struct ParamList {
	std::vector<Param> out, in;
};

struct Symbol {
	enum SymbolKind { TERM, NTERM, ACTION } kind;
	std::string name;
	std::vector<Choice> choices;
	std::unique_ptr<std::vector<Choice>> choices_core;
	std::set<Symbol*> first, follow;
	ParamList params;
	std::vector<Param> locals;
	union {
		const char *sp = nullptr; // TERM: semantic predicate
		const char *action; // ACTION (inline)
	};
	int id = -1;
	bool nullable = false;
	bool defined = false;
	bool weak = false;
	int opening_sym();
	Symbol(SymbolKind kind, const std::string &name);
	~Symbol();
};

struct ArgList {
	std::vector<std::string> out, in;
};

struct Instance {
	Symbol *sym;
	std::unique_ptr<ArgList> args;
	Instance(Symbol *sym, const ArgList &args);
	Instance(Symbol *sym);
};

extern std::map<std::string, Symbol*> term_dict, nterm_dict, action_dict;
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

template <typename F>
void for_each_action(F f) {
	for (auto &pair: action_dict)
		f(pair.second);
}

void for_each_reachable_nterm(std::function<void(Symbol*)>);
