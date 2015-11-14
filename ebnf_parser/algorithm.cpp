#include <algorithm>
#include <cassert>
#include <cstdio>
#include "ebnf.h"

using namespace std;

void print_production(Symbol *nterm, const vector<Symbol*> &body);

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
					if (all_of(choice.begin(), choice.end(), [](Symbol *s){return s->nullable;})) {
						nterm->nullable = true;
						changed = true;
					}
				}
				for (Symbol *s: choice) {
					if (insert_all(nterm->first, s->first))
						changed = true;
					if (!s->nullable)
						break;
				}
				for (auto it1 = choice.begin(); it1 != choice.end(); it1++) {
					Symbol *s1 = *it1, *s2;
					if (s1->kind == Symbol::TERM)
						continue;
					auto it2 = next(it1);
					while (it2 != choice.end()) {
						s2 = *it2;
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

void detect_useless_rules()
{
	NTermVisitor visitor([](Symbol *){});
	visitor.visit(top);
	for_each_nterm([&](Symbol *nterm) {
		if (!visitor.vis.count(nterm))
			fprintf(stderr, "warning: <%s> is useless\n", nterm->name.c_str());
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
					Symbol *s = *it;
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

set<Symbol*> first_of_production(Symbol *nterm, const vector<Symbol*> &body)
{
	set<Symbol*> f;
	auto it = body.begin();
	while (it != body.end()) {
		Symbol *s = *it;
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
	detect_useless_rules();
	if (!check_left_recursion())
		ans = false;
	for_each_nterm([&](Symbol *nterm) {
		map<Symbol*, vector<Symbol*>*> m;
		for (auto &choice: nterm->choices) {
			set<Symbol*> f = first_of_production(nterm, choice);
			for (Symbol *s: f) {
				if (m[s]) {
					printf("conflict on %s:\n", s->name.c_str());
					print_production(nterm, *m[s]);
					print_production(nterm, choice);
					ans = false;
				} else {
					m[s] = &choice;
				}
			}
		}
	});
	return ans;
}
