struct TranslateEnv;

struct Operand {
	enum Kind {
		IMM,
		TEMP,
		MEM,
		LABEL,
	} kind;
	int size;
	void print() const
	{
		printf("%s", tostr().c_str());
	}
	virtual std::string tostr() const = 0;
	bool isimm()   const { return kind == IMM;   }
	bool istemp()  const { return kind == TEMP;  }
	bool ismem()   const { return kind == MEM;   }
	bool islabel() const { return kind == LABEL; }
protected:
	Operand(Kind kind, int size): kind(kind), size(size) {}
};

struct ImmOperand: Operand
{
	int val;
	ImmOperand(int val): Operand(IMM, 4), val(val) {}
	std::string tostr() const override;
};

struct TempOperand: Operand
{
	int id;
	TempOperand(int size, int id): Operand(TEMP, size), id(id) {}
	std::string tostr() const override;
};

struct LabelOperand: Operand
{
	std::string label;
	LabelOperand(const std::string &label): Operand(LABEL, 4), label(label) {}
	std::string tostr() const override;
};

struct MemOperand: Operand
{
	Operand *base;
	int offset;
	Operand *index;
	int scale;
	MemOperand(int size,
		   Operand *base,
		   int offset,
		   Operand *index,
		   int scale):
		Operand(MEM, size),
		base(base),
		offset(offset),
		index(index),
		scale(scale) {}
	MemOperand(int size, Operand *base, int offset):
		MemOperand(size, base, offset, nullptr, 0) {}
	MemOperand(int size, Operand *base):
		MemOperand(size, base, 0, nullptr, 0) {}
	std::string tostr() const override;
};

// at most one temporary (non-physreg) is defined at a time
struct Quad {
	enum Op {
		ADD,
		SUB,
		MUL,
		DIV,
		BEQ,
		BNE,
		BLT,
		BGE,
		BGT,
		BLE,
		NEG,
		MOV,
		JMP,
		CALL,
		LEA,
		PUSH,
		INC,
		DEC,
		SEX,
		CDQ,
		ADD3,
		SUB3,
		MUL3,
		DIV3,
		NEG2,
		LABEL,
		PHI,
		SYNCM,
		SYNCR,
	} op;
	Operand *c;
	union {
		Operand *a;
		Operand **args; // NULL-terminated
	};
	Operand *b;
	Quad(Op op, Operand *c, Operand *a, Operand *b):
		op(op), c(c), a(a), b(b) {}
	Quad(Op op, Operand *c, Operand *a): Quad(op, c, a, nullptr) {}
	Quad(Op op, Operand *c): Quad(op, c, nullptr, nullptr) {}
	Quad(Op op): Quad(op, nullptr, nullptr, nullptr) {}
	Quad(Op op, Operand *c, Operand **args): op(op), c(c), args(args) {}
	std::string tostr() const;
	bool isjump() const
	{
		return op == JMP;
	}
	bool isbranch() const
	{
		switch (op) {
		case BEQ:
		case BNE:
		case BLT:
		case BGE:
		case BGT:
		case BLE:
			return true;
		default:
			return false;
		}
		return false;
	}
	bool is_jump_or_branch() const
	{
		switch (op) {
		case JMP:
		case BEQ:
		case BNE:
		case BLT:
		case BGE:
		case BGT:
		case BLE:
			return true;
		default:
			return false;
		}
	}
};

struct Symbol;
struct SymbolTable;
struct ProcSymbol;
struct Expr;

struct TranslateOptions {
	int optimize = 0; // optimization level
	std::string out_fname;
};

struct VarSymbol;
class TranslateEnv {
	SymbolTable *symtab;
	std::string procname; // decorated name
	FILE *outfp;
	int framesize = 0;
	int level; // 1 for top level
	int tempid = 0;
	int labelid = 0;
	std::vector<int> temp_reg;
	std::vector<int> temp_scalar;
	std::vector<int> temp_offset; // for spilled temporaries
	std::vector<VarSymbol*> params;
	std::vector<VarSymbol*> vars; // local vars
	std::vector<VarSymbol*> scalar_var;
	std::vector<TempOperand*> temps;
	TranslateEnv *up;
	int scalar_id; // after construction, number of visible scalars (explicitly defined)

	void emit(const char *ins, Operand *dst, Operand *src);
	void emit(const char *ins, Operand *dst);
	void emit(const char *ins);
	Operand *resolve(Operand *o);
	TempOperand *totemp(Operand *o); // emit quads to load o into a temporary
	void sync_mem(int a);
	void sync_reg(int a);

public:
	std::vector<Quad> quads;
	std::vector<TempOperand*> scalar_temp;
	const TranslateOptions *opt;

	TempOperand *newtemp(int size);
	TempOperand *newtemp_scalar(int size, int scalar);
	LabelOperand *newlabel();
	Symbol *lookup(const std::string &name) const;
	Operand *translate_sym(const Symbol *sym);
	MemOperand *translate_varsym(const VarSymbol *sym);
	MemOperand *translate_lvalue(const Expr *e);
	void translate_call(ProcSymbol *proc, const std::vector<std::unique_ptr<Expr>> &args);
	int physreg(const TempOperand *t);
	Operand *resize(int size, Operand *o);
	TranslateEnv(SymbolTable *symtab,
		     const std::string &procname,
		     FILE *outfp,
		     const std::vector<VarSymbol*> &params,
		     const std::vector<VarSymbol*> &vars,
		     TranslateEnv *up,
		     const TranslateOptions *opt);
	void gencode();
	void rewrite();
	void rewrite_mem(MemOperand *m);
	void try_rewrite_mem(Operand *o);
	void allocaddr();
	void assign_scalar_id();
	void optimize();
	void sync(Quad::Op op);
	void insert_sync();
	void lower();
	void dump_cfg();
	void dump_quads();
};

extern const char *regname4[8];
extern const char *regname1[4];

extern TempOperand *eax, *ecx, *edx, *ebx, *esp, *ebp, *esi, *edi;
TempOperand *getphysreg(int size, int id);

struct Block;
void translate_all(std::unique_ptr<Block> &&blk, const TranslateOptions *options);

void todo(const char *file, int line, const char *msg);
#define TODO(msg) todo(__FILE__, __LINE__, msg)

#define astemp static_cast<TempOperand*>
#define asmem  static_cast<MemOperand*>

#ifdef __CYGWIN__
# define EP "_"
#else
# define EP
#endif
