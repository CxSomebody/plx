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
					Expr *left; // null for unary
					Expr *right;
				}; // binary & unary
				struct {
					Expr *func;
					std::vector<Expr*> *args;
				}; // APPLY
			};
		}; // COMP
	};
};

struct Stmt {
	enum Kind {
		ASSIGN,
		CALL,
	} kind;
	union {
		struct {
			Expr *var;
			Expr *val;
		}; // ASSIGN
		struct {
			Symbol *proc;
			std::vector<Expr*> *args;
		}; // CALL
	};
};

typedef std::vector<Expr*> ExprList;

Expr *binary_expr(Expr::Op op, Expr *left, Expr *right);
Expr *unary_expr(Expr::Op op, Expr *right);
Expr *ident_expr(const std::string &name);
Expr *lit_expr(int lit);
Expr *apply_expr(Expr *func, const std::vector<Expr*> &args);
void print_expr(Expr *e);

Stmt *assign_stmt(Expr *var, Expr *val);
Stmt *call_stmt(const std::string &name, const std::vector<Expr*> &args);
void print_stmt(Stmt *s);
