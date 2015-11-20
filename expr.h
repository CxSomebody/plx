struct Expr {
	enum Kind {
		SYM,
		LIT,
		COMP,
	} kind;
	enum Op {
		ADD,
		SUB,
		MUL,
		DIV,
		NEG,
		INDEX,
		APPLY,
	};
	union {
		Symbol *sym;
		int lit;
		struct {
			Op op;
			union {
				struct {
					Expr *left;
					Expr *right;
				};
				std::vector<Expr*> *args;
			};
		};
	};
};

Expr *binary_expr(Expr::Op op, Expr *left, Expr *right);
Expr *ident_expr(const std::string &name);
Expr *lit_expr(int lit);
Expr *index_expr(Expr *base, Expr *index);
