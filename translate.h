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
	bool istemp() const { return kind == TEMP; }
	bool ismem () const { return kind == MEM ; }
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
	TempOperand *baset;
	LabelOperand *basel;
	int offset;
	TempOperand *index;
	int scale;
	MemOperand(int size,
		   TempOperand *baset,
		   int offset,
		   TempOperand *index,
		   int scale):
		Operand(MEM, size),
		baset(baset),
		basel(nullptr),
		offset(offset),
		index(index),
		scale(scale) {}
	MemOperand(int size, TempOperand *baset, int offset):
		MemOperand(size, baset, offset, nullptr, 0) {}
	MemOperand(int size,
		   LabelOperand *basel,
		   int offset,
		   TempOperand *index,
		   int scale):
		Operand(MEM, size),
		baset(nullptr),
		basel(basel),
		offset(offset),
		index(index),
		scale(scale) {}
	MemOperand(int size, TempOperand *baset):
		MemOperand(size, baset, 0, nullptr, 0) {}
	MemOperand(int size, LabelOperand *basel):
		MemOperand(size, basel, 0, nullptr, 0) {}
	void print() const override
	{
		printf("%d", size);
		putchar('[');
		bool sep = false;
		if (baset) {
			baset->print();
			sep = true;
		}
		if (basel) {
			if (sep)
				putchar('+');
			basel->print();
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

class TranslateEnv {
	SymbolTable *symtab;
	std::string procname;
	FILE *outfp;
	int framesize;
	int level;
	int tempid = 0;
	int labelid = 0;
	std::vector<int> temp_reg;
	void emit(const char *ins, Operand *dst, Operand *src);
	void emit(const char *ins, Operand *dst);
	void emit(const char *ins);
	void emit_mov(Operand *dst, Operand *src);
	void resolve(Operand *o);
public:
	std::vector<Quad> quads;
	TempOperand *newtemp(int size);
	LabelOperand *newlabel();
	Symbol *lookup(const std::string &name) const;
	Operand *translate_sym(Symbol *sym);
	void translate_call(ProcSymbol *proc, const std::vector<std::unique_ptr<Expr>> &args);
	int physreg(const TempOperand *t);
	TranslateEnv(SymbolTable *symtab, const std::string &procname, FILE *outfp, int framesize);
	void gencode();
};

extern const char *regname4[8];
extern const char *regname1[4];

extern TempOperand *eax, *ecx, *edx, *ebx, *esp, *ebp, *esi, *edi;
TempOperand *getphysreg(int size, int id);

void todo(const char *file, int line, const char *msg);
#define TODO(msg) todo(__FILE__, __LINE__, msg)

#ifdef __CYGWIN__
# define EP "_"
#else
# define EP
#endif
