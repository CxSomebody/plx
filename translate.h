struct Operand {
	enum Kind {
		IMM,
		TEMP,
		MEM,
		LABEL,
		LIST, // for function calls
	} kind;
	virtual void print() const = 0;
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
};

struct TempOperand: Operand
{
	int id;
	TempOperand(int id): Operand(TEMP), id(id) {}
	void print() const override
	{
		if (id >= 0) printf("$%d", id);
		else printf("%%%d", ~id); // physical register
	}
};

struct LabelOperand: Operand
{
	std::string label;
	LabelOperand(const std::string &label): Operand(LABEL), label(label) {}
	void print() const override
	{
		printf("%s", label.c_str());
	}
};

struct MemOperand: Operand
{
	int size;
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
		size(size),
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
		size(size),
		baset(nullptr),
		basel(basel),
		offset(offset),
		index(index),
		scale(scale) {}
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
};

struct Symbol;
struct SymbolTable;
class TranslateEnv {
	SymbolTable *symtab;
	int tempid = 0;
	int labelid = 0;
public:
	std::vector<Quad> quads;
	TempOperand *newtemp();
	LabelOperand *newlabel();
	Symbol *lookup(const std::string &name) const;
	TranslateEnv(SymbolTable *symtab): symtab(symtab) {}
	int level() const;
	void gencode();
};
