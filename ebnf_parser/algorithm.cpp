#include <algorithm>
#include <cassert>
#include <cstdio>
#include "ebnf.h"

using namespace std;

void print_production(Symbol *nterm, const Choice &body, FILE *fp);

static bool insert_all(set<Symbol*> &a, const set<Symbol*> &b)
{
	size_t old_size = a.size();
	a.insert(b.begin(), b.end());
	return a.size() > old_size;
}

void compute_first_follow()
{
	for_each_term([](Symbol *term) {
		term->first.insert(term);
	});
	bool changed;
	do {
		changed = false;
		for_each_nterm([&](Symbol *nterm) {
			for (auto &choice: nterm->choices) {
				if (!nterm->nullable) {
					if (all_of(choice.begin(), choice.end(), [](Instance *inst){return inst->sym->nullable;})) {
						nterm->nullable = true;
						changed = true;
					}
				}
				for (Instance *inst: choice) {
					Symbol *s = inst->sym;
					if (insert_all(nterm->first, s->first))
						changed = true;
					if (!s->nullable)
						break;
				}
				for (auto it1 = choice.begin(); it1 != choice.end(); it1++) {
					Symbol *s1 = (*it1)->sym, *s2;
					if (s1->kind != Symbol::NTERM)
						continue;
					auto it2 = next(it1);
					while (it2 != choice.end()) {
						s2 = (*it2)->sym;
						if (!s2->nullable)
							break;
						it2++;
					}
					if (it2 == choice.end()) {
						if (insert_all(s1->follow, nterm->follow))
							changed = true;
					} else {
						if (insert_all(s1->follow, s2->first))
							changed = true;
					}
				}
			}
		});
	} while (changed);
}

static void number_nterms()
{
	int id = 0;
	for_each_reachable_nterm([&](Symbol *nterm) {
		nterm->id = id++;
	});
}

// true if no left recursion
bool check_left_recursion()
{
	struct {
		set<Symbol*> vis;
		bool ans = true;
		void visit(Symbol *nterm) {
			assert(nterm->kind == Symbol::NTERM);
			if (vis.count(nterm)) {
				fprintf(stderr, "error: <%s> is left-recursive\n", nterm->name.c_str());
				ans = false;
				return;
			}
			vis.insert(nterm);
			for (auto &choice: nterm->choices) {
				auto it = choice.begin();
				while (it != choice.end()) {
					Symbol *s = (*it)->sym;
					if (s->kind == Symbol::NTERM)
						visit(s);
					if (!s->nullable)
						break;
					it++;
				}
			}
		}
	} visitor;
	visitor.visit(top);
	return visitor.ans;
}

set<Symbol*> first_of_production(Symbol *nterm, const Choice &body)
{
	set<Symbol*> f;
	auto it = body.begin();
	while (it != body.end()) {
		Symbol *s = (*it)->sym;
		f.insert(s->first.begin(), s->first.end());
		if (!s->nullable)
			break;
		it++;
	}
	if (it == body.end())
		f.insert(nterm->follow.begin(), nterm->follow.end());
	return f;
}

bool check_grammar()
{
	// FIRST(X->y) = if nullable(y) then FIRST(y) ∪ FOLLOW(X) else FIRST(y)
	/* for each nonterminal X, for each pair of distinct productions
	   X->y1 and X->y2, FIRST(X->y1) ∩ FIRST(X->y2) = ∅ */
	bool ans = true;
	number_nterms();
	// detect unreachable nonterminals
	for_each_nterm([](Symbol *nterm) {
		if (nterm->id < 0)
			fprintf(stderr, "warning: <%s> is useless\n", nterm->name.c_str());
	});
	if (!check_left_recursion())
		ans = false;
	for_each_nterm([&](Symbol *nterm) {
		map<Symbol*, Choice*> m;
		for (auto &choice: nterm->choices) {
			set<Symbol*> f = first_of_production(nterm, choice);
			for (Symbol *s: f) {
				if (m[s]) {
					fprintf(stderr, "conflict on %s:\n", s->name.c_str());
					print_production(nterm, *m[s], stderr);
					print_production(nterm, choice, stderr);
					ans = false;
				} else {
					m[s] = &choice;
				}
			}
		}
	});
	return ans;
}
