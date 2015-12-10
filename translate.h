struct TranslateEnv;

struct Operand {
	enum Kind {
		IMM,
		TEMP,
		MEM,
		LABEL,
	} kind;
	int size;
	virtual void print() const = 0;
	virtual std::string gencode() const = 0;
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
	void print() const override
	{
		printf("%d", val);
	}
	std::string gencode() const override;
};

struct TempOperand: Operand
{
	int id;
	TempOperand(int size, int id): Operand(TEMP, size), id(id) {}
	void print() const override
	{
		void printtemp(int);
		printtemp(id);
	}
	std::string gencode() const override;
};

struct LabelOperand: Operand
{
	std::string label;
	LabelOperand(const std::string &label): Operand(LABEL, 4), label(label) {}
	void print() const override
	{
		printf("%s", label.c_str());
	}
	std::string gencode() const override;
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
	void print() const override
	{
		printf("%d", size);
		putchar('[');
		bool sep = false;
		if (base) {
			base->print();
			sep = true;
		}
		if (offset) {
			printf(sep?"%+d":"%d", offset);
			sep = true;
		}
		if (index) {
			if (sep)
				putchar('+');
			index->print();
			printf("*%d", scale);
		}
		putchar(']');
	}
	std::string gencode() const override;
};

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
		LABEL,
	} op;
	Operand *c, *a, *b;
	Quad(Op op, Operand *c, Operand *a, Operand *b):
		op(op), c(c), a(a), b(b) {}
	Quad(Op op, Operand *c, Operand *a): Quad(op, c, a, nullptr) {}
	Quad(Op op, Operand *c): Quad(op, c, nullptr, nullptr) {}
	Quad(Op op): Quad(op, nullptr, nullptr, nullptr) {}
	void print() const;
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
};

struct VarSymbol;
class TranslateEnv {
	SymbolTable *symtab;
	std::string procname;
	FILE *outfp;
	int framesize = 0;
	int level;
	int tempid = 0;
	int labelid = 0;
	std::vector<int> temp_reg;
	std::vector<int> tempsize;
	std::vector<VarSymbol*> params;
	std::vector<VarSymbol*> vars;
	std::vector<TempOperand*> scalar_temp;
	TranslateEnv *up;
	const TranslateOptions *opt;
	int scalar_id;
	void emit(const char *ins, Operand *dst, Operand *src);
	void emit(const char *ins, Operand *dst);
	void emit(const char *ins);
	void emit_mov(Operand *dst, Operand *src);
	Operand *resolve(Operand *o);
	TempOperand *totemp(Operand *o);
public:
	std::vector<Quad> quads;
	TempOperand *newtemp(int size);
	LabelOperand *newlabel();
	Symbol *lookup(const std::string &name) const;
	Operand *translate_sym(const Symbol *sym);
	MemOperand *translate_sym_mem(const VarSymbol *sym);
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
	void allocaddr();
	void assign_scalar_id();
};

extern const char *regname4[8];
extern const char *regname1[4];

extern TempOperand *eax, *ecx, *edx, *ebx, *esp, *ebp, *esi, *edi;
TempOperand *getphysreg(int size, int id);

struct Block;
void translate_all(std::unique_ptr<Block> &&blk, const TranslateOptions *options);

void todo(const char *file, int line, const char *msg);
#define TODO(msg) todo(__FILE__, __LINE__, msg)

#ifdef __CYGWIN__
# define EP "_"
#else
# define EP
#endif
