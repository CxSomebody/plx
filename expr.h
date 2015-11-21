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

struct Cond {
	enum Op {
		EQ, NE, LT, GE, GT, LE
	} op;
	Expr *left, *right;
};

struct Stmt {
	enum Kind {
		COMP,
		ASSIGN,
		CALL,
		IF,
	} kind;
	union {
		struct {
			std::vector<Stmt*> *body;
		}; // COMP
		struct {
			Expr *var;
			Expr *val;
		}; // ASSIGN
		struct {
			Symbol *proc;
			std::vector<Expr*> *args;
		}; // CALL
		struct {
			Cond *cond;
			Stmt *st;
			Stmt *sf;
		}; // IF
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
Stmt *if_stmt(Cond *cond, Stmt *st);
Stmt *if_stmt(Cond *cond, Stmt *st, Stmt *sf);
Stmt *comp_stmt(const std::vector<Stmt*> &body);
void print_stmt(Stmt *s);

Cond *cond(Cond::Op op, Expr *left, Expr *right);
void print_cond(Cond *c);
