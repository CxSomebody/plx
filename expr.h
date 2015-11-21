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
		Symbol *sym; // SYM
		int lit; // LIT
		struct {
			Op op;
			union {
				struct {
					Expr *left;
					Expr *right;
				}; // binary & unary
				std::vector<Expr*> *args; // APPLY
			};
		}; // COMP
	};
};

struct Stmt {
	enum Kind {
		ASSIGN,
	} kind;
	union {
		struct {
			Expr *var;
			Expr *val;
		}; // ASSIGN
	};
};

Expr *binary_expr(Expr::Op op, Expr *left, Expr *right);
Expr *unary_expr(Expr::Op op, Expr *right);
Expr *ident_expr(const std::string &name);
Expr *lit_expr(int lit);
void print_expr(Expr *e);

Stmt *assign_stmt(Expr *var, Expr *val);
