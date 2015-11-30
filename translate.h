struct TranslateEnv;

struct Operand {
	enum Kind {
		IMM,
		TEMP,
		MEM,
		LABEL,
		LIST, // for function calls
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
		if (id >= 0) printf("$%d", id);
		else printf("%%%d", ~id); // physical register
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

struct ListOperand: Operand
{
	std::vector<Operand*> list;
	ListOperand(): Operand(LIST) {}
	void print() const override
	{
		bool sep = false;
		for (Operand *o: list) {
			if (sep)
				printf(", ");
			o->print();
			sep = true;
		}
	}
	int size() const override
	{
		return 0;
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
		LABEL,
		LEA,
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
	FILE *outfp;
	int tempid = 0;
	int labelid = 0;
	int reg_temp[8];
	void emit(const std::string &ins, const std::string &dst, const std::string &src);
	void emit(const std::string &ins, const std::string &dst);
	void allocreg(int t);
public:
	std::vector<Quad> quads;
	std::vector<int> temp_reg;
	TempOperand *newtemp(int size);
	LabelOperand *newlabel();
	Symbol *lookup(const std::string &name) const;
	TranslateEnv(SymbolTable *symtab, FILE *outfp): symtab(symtab), outfp(outfp) {}
	int level() const;
	void gencode();
};

void todo(const char *file, int line, const char *msg);
#define TODO(msg) todo(__FILE__, __LINE__, msg)
