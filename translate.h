struct TranslateEnv;

struct Operand {
	enum Kind {
		IMM,
		TEMP,
		MEM,
		LABEL,
	} kind;
	virtual void print() const = 0;
	virtual std::string gencode(TranslateEnv &env) const = 0;
	virtual int size() const = 0;
protected:
	Operand(Kind kind): kind(kind) {}
};

struct ImmOperand: Operand
{
	int val;
	ImmOperand(int val): Operand(IMM), val(val) {}
	void print() const override
	{
		printf("%d", val);
	}
	int size() const override
	{
		return 4;
	}
	std::string gencode(TranslateEnv &env) const override;
};

struct TempOperand: Operand
{
	int id;
	int _size;
	TempOperand(int id, int size): Operand(TEMP), id(id), _size(size) {}
	void print() const override
	{
		void printtemp(int);
		printtemp(id);
	}
	int size() const override
	{
		return _size;
	}
	std::string gencode(TranslateEnv &env) const override;
};

struct LabelOperand: Operand
{
	std::string label;
	LabelOperand(const std::string &label): Operand(LABEL), label(label) {}
	void print() const override
	{
		printf("%s", label.c_str());
	}
	int size() const override
	{
		return 4; // a label is a pointer
	}
	std::string gencode(TranslateEnv &env) const override;
};

struct MemOperand: Operand
{
	int _size;
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
		Operand(MEM),
		_size(size),
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
		Operand(MEM),
		_size(size),
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
		printf("%d", _size);
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
	int size() const override
	{
		return _size;
	}
	std::string gencode(TranslateEnv &env) const override;
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
		LABEL,
	} op;
	Operand *c, *a, *b;
	Quad(Op op, Operand *c, Operand *a, Operand *b):
		op(op), c(c), a(a), b(b) {}
	Quad(Op op, Operand *c, Operand *a): Quad(op, c, a, nullptr) {}
	Quad(Op op, Operand *c): Quad(op, c, nullptr, nullptr) {}
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

class TranslateEnv {
	SymbolTable *symtab;
	std::string procname;
	FILE *outfp;
	int framesize;
	int tempid = 0;
	int labelid = 0;
	std::vector<int> temp_reg;
	void emit(const char *ins, Operand *dst, Operand *src);
	void emit(const char *ins, Operand *dst);
	void emit(const char *ins);
	void emit_mov(Operand *dst, Operand *src);
public:
	std::vector<Quad> quads;
	TempOperand *newtemp(int size);
	LabelOperand *newlabel();
	Symbol *lookup(const std::string &name) const;
	int physreg(const TempOperand *t);
	TranslateEnv(SymbolTable *symtab, const std::string &procname, FILE *outfp, int framesize): symtab(symtab), procname(procname), outfp(outfp), framesize(framesize) {}
	int level() const;
	void gencode();
};

extern const char *regname[8];

extern TempOperand *eax, *ecx, *edx, *ebx, *esp, *ebp, *esi, *edi;

void todo(const char *file, int line, const char *msg);
#define TODO(msg) todo(__FILE__, __LINE__, msg)
