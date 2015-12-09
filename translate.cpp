#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"
#include "translate.h"

using namespace std;

static const char *opstr[] = {
	"+=",
	"-=",
	"*=",
	nullptr,
	"=",
	"<>",
	"<",
	">=",
	">",
	"<=",
};

TranslateEnv::TranslateEnv(SymbolTable *symtab,
			   const std::string &procname,
			   FILE *outfp,
			   const vector<VarSymbol*> &vars):
	symtab(symtab),
	procname(procname),
	outfp(outfp),
	vars(vars)
{
	level = symtab->level;
	if (level < 1)
		level = 1;
}

Symbol *TranslateEnv::lookup(const std::string &name) const
{
	return symtab->lookup(name);
}

static TempOperand physreg4[8] = {
	{4,~0}, {4,~1}, {4,~2}, {4,~3},
	{4,~4}, {4,~5}, {4,~6}, {4,~7},
};

static TempOperand physreg1[4] = {
	{1,~0}, {1,~1}, {1,~2}, {1,~3},
};

TempOperand *eax = &physreg4[0];
TempOperand *ecx = &physreg4[1];
TempOperand *edx = &physreg4[2];
TempOperand *ebx = &physreg4[3];
TempOperand *esp = &physreg4[4];
TempOperand *ebp = &physreg4[5];
TempOperand *esi = &physreg4[6];
TempOperand *edi = &physreg4[7];

TempOperand *getphysreg(int size, int id)
{
	assert(id >= 0);
	if (size == 4) {
		assert(id < 8);
		return &physreg4[id];
	}
	if (size == 1) {
		assert(id < 4);
		return &physreg1[id];
	}
	assert(0);
}

#if 0
TempOperand *al = &physreg[8];
TempOperand *cl = &physreg[9];
TempOperand *dl = &physreg[10];
TempOperand *bl = &physreg[11];
#endif

void todo(const char *file, int line, const char *msg)
{
	fprintf(stderr, "%s: %d: TODO: %s\n", file, line, msg);
	abort();
}

void Quad::print() const
{
	switch (op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MUL:
		c->print();
		printf(" %s ", opstr[op]);
		a->print();
		break;
	case Quad::DIV:
		printf("div ");
		c->print();
		break;
	case Quad::NEG:
		printf("neg ");
		c->print();
		break;
	case Quad::MOV:
		c->print();
		printf(" = ");
		a->print();
		break;
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
		printf("goto ");
		c->print();
		printf(" if ");
		a->print();
		printf(" %s ", opstr[op]);
		b->print();
		break;
	case Quad::CALL:
		printf("call ");
		c->print();
		break;
	case Quad::LABEL:
		c->print();
		putchar(':');
		break;
	case Quad::JMP:
		printf("goto ");
		c->print();
		break;
	case Quad::LEA:
		c->print();
		printf(" = ");
		putchar('&');
		a->print();
		break;
	case Quad::PUSH:
		printf("push ");
		c->print();
		break;
	case Quad::INC:
		printf("inc ");
		c->print();
		break;
	case Quad::DEC:
		printf("dec ");
		c->print();
		break;
	case Quad::SEX:
		c->print();
		printf(" = sex ");
		a->print();
		break;
	case Quad::CDQ:
		printf("cdq");
		break;
	default:
		assert(0);
	}
}

TempOperand *TranslateEnv::newtemp(int size)
{
	tempsize.push_back(size);
	return new TempOperand(size, tempid++);
}

LabelOperand *TranslateEnv::newlabel()
{
	char tmp[16];
	sprintf(tmp, ".l%d", labelid++);
	return new LabelOperand(tmp);
}

#if 0
TempOperand *astemp(Operand *o, TranslateEnv &env)
{
	if (o->kind != Operand::TEMP) {
		TempOperand *t = env.newtemp(o->size);
		env.quads.emplace_back(Quad::MOV, t, o);
		return t;
	}
	return static_cast<TempOperand*>(o);
}
#endif

TempOperand *TranslateEnv::totemp(Operand *o)
{
	if (o->kind != Operand::TEMP) {
		TempOperand *t = newtemp(o->size);
		quads.emplace_back(Quad::MOV, t, o);
		return t;
	}
	return static_cast<TempOperand*>(o);
}

Operand *resize(TranslateEnv &env, int size, Operand *o)
{
	if (size == o->size)
		return o;
	if (size > o->size) {
		TempOperand *t = env.newtemp(size);
		env.quads.emplace_back(Quad::SEX, t, o);
		return t;
	}
	// shrink the operand
	switch (o->kind) {
	case Operand::IMM:
		{
			ImmOperand *i = new ImmOperand(*static_cast<ImmOperand*>(o));
			i->size = size;
			i->val &= (1 << size*8)-1;
			return i;
		}
	case Operand::TEMP:
		// TODO what if physreg for t is none of e[acdb]x
		{
			TempOperand *t = new TempOperand(size, static_cast<TempOperand*>(o)->id);
			t->size = size;
			return t;
		}
	case Operand::MEM:
		{
			MemOperand *m = new MemOperand(*static_cast<MemOperand*>(o));
			m->size = size;
			return m;
		}
	default:
		assert(0);
	}
}

Operand *TranslateEnv::translate_sym(Symbol *sym)
{
	if (sym->kind == Symbol::VAR) {
		VarSymbol *vs = static_cast<VarSymbol*>(sym);
		MemOperand *m;
		if (vs->level) {
			// local var
			TempOperand *bp;
			if (vs->level == level) {
				bp = ebp;
			} else {
				int leveldiff = level-vs->level;
				bp = newtemp(4);
				quads.emplace_back(Quad::MOV, bp, new MemOperand(4, ebp, 8+4*(leveldiff-1)));
			}
			m = new MemOperand(vs->isref ? 4 : vs->type->size(), bp, vs->offset);
			if (vs->isref) {
				// m is a pointer
				TempOperand *t = newtemp(4);
				quads.emplace_back(Quad::MOV, t, m);
				m = new MemOperand(vs->type->size(), t);
			}
		} else {
			// global var
			m = new MemOperand(vs->type->size(), new LabelOperand('$'+vs->name));
		}
		return m;
	}
	if (sym->kind == Symbol::PROC) {
		// address of function
		return new LabelOperand(static_cast<ProcSymbol*>(sym)->decorated_name);
	}
	assert(sym->kind == Symbol::CONST);
	return new ImmOperand(static_cast<ConstSymbol*>(sym)->val);
}

void TranslateEnv::translate_call(ProcSymbol *proc, const vector<unique_ptr<Expr>> &args)
{
	int i=0;
	for (auto it = args.rbegin(); it != args.rend(); it++) {
		const unique_ptr<Expr> &arg = *it;
		Operand *o = arg->translate(*this);
		if (proc->params[i].byref) {
			TempOperand *addr = newtemp(4);
			assert(o->kind == Operand::MEM);
			static_cast<MemOperand*>(o)->size = 0;
			quads.emplace_back(Quad::LEA, addr, o);
			o = addr;
		}
		quads.emplace_back(Quad::PUSH, resize(*this, 4, o));
		i++;
	}
	assert(proc->level > 0 && proc->level <= level+1);
	for (int i=1; i<proc->level; i++)
		quads.emplace_back(Quad::PUSH, i == level ?
				   static_cast<Operand*>(ebp) :
				   static_cast<Operand*>(new MemOperand(4, ebp, 8+(i-1)*4)));
	//printf("proc %s level=%d\n", proc->name.c_str(), proc->level);
	int spinc = (args.size()+(proc->level-1))*4;
	quads.emplace_back(Quad::CALL, translate_sym(proc));
	if (spinc)
		quads.emplace_back(Quad::ADD, esp, new ImmOperand(spinc));
}

Operand *SymExpr::translate(TranslateEnv &env) const
{
	return env.translate_sym(sym);
}

Operand *LitExpr::translate(TranslateEnv &env) const
{
	return new ImmOperand(lit);
}

Operand *BinaryExpr::translate(TranslateEnv &env) const
{
	Operand *c, *a, *b;
	Quad::Op qop;
	switch (op) {
	case ADD: qop = Quad::ADD; break;
	case SUB: qop = Quad::SUB; break;
	case MUL: qop = Quad::MUL; break;
	case DIV: qop = Quad::DIV; break;
	default: assert(0);
	}
	c = env.newtemp(type->size());
	a = resize(env, c->size, left ->translate(env));
	b = resize(env, c->size, right->translate(env));
	if (op == DIV) {
		if (c->size == 4) {
			env.quads.emplace_back(Quad::MOV, eax, a);
			env.quads.emplace_back(Quad::CDQ);
			env.quads.emplace_back(Quad::DIV, b);
			env.quads.emplace_back(Quad::MOV, c, eax);
		} else {
			TODO("DIV with size(c) != 4");
		}
	} else {
		env.quads.emplace_back(Quad::MOV, c, a);
		env.quads.emplace_back(qop, c, b);
	}
	return c;
}

Operand *IndexExpr::translate(TranslateEnv &env) const
{
	Operand *c;
	assert(array->kind == SYM);
	Symbol *arraysym = static_cast<SymExpr*>(array.get())->sym;
	assert(arraysym->kind == Symbol::VAR);
	Type *arrayty = static_cast<VarSymbol*>(arraysym)->type;
	assert(arrayty->kind == Type::ARRAY);
	int scale = static_cast<ArrayType*>(arrayty)->elemtype->size();
	c = array->translate(env);
	assert(c->kind == Operand::MEM);
	MemOperand *moarray = static_cast<MemOperand*>(c);
	assert(!moarray->index);
	moarray->size = type->size();
	Operand *oindex = index->translate(env);
	if (oindex->kind == Operand::MEM) {
		TempOperand *tmp = env.newtemp(index->type->size());
		env.quads.emplace_back(Quad::MOV, tmp, oindex);
		oindex = tmp;
	}
	if (oindex->kind == Operand::IMM) {
		moarray->offset += static_cast<ImmOperand*>(oindex)->val * scale;
	} else if (oindex->kind == Operand::TEMP) {
		moarray->index = static_cast<TempOperand*>(oindex);
		moarray->scale = scale;
	} else {
		assert(0);
	}
	return c;
}

Operand *UnaryExpr::translate(TranslateEnv &env) const
{
	Quad::Op qop;
	switch (op) {
	case NEG: qop = Quad::NEG; break;
	default: assert(0);
	}
	Operand *c = env.newtemp(type->size());
	Operand *a = sub->translate(env);
	assert(c->size == a->size);
	env.quads.emplace_back(Quad::MOV, c, a);
	env.quads.emplace_back(qop, c);
	return c;
}

Operand *ApplyExpr::translate(TranslateEnv &env) const
{
	env.translate_call(func, args);
	return eax;
}

void EmptyStmt::translate(TranslateEnv &env) const
{
	// do nothing
}

void CompStmt::translate(TranslateEnv &env) const
{
	for (const unique_ptr<Stmt> &s: body)
		s->translate(env);
}

void AssignStmt::translate(TranslateEnv &env) const
{
	Operand *ovar;
	Symbol *varsym;
	if (var->kind == Expr::SYM &&
	    (varsym = static_cast<SymExpr*>(var.get())->sym)->kind == Symbol::PROC) {
		ovar = env.translate_sym(env.lookup(varsym->name+'$'));
	} else {
		ovar = var->translate(env);
	}
	Operand *oval = val->translate(env);
	oval = resize(env, ovar->size, oval);
	env.quads.emplace_back(Quad::MOV, ovar, oval);
}

void CallStmt::translate(TranslateEnv &env) const
{
	env.translate_call(proc, args);
}

void IfStmt::translate(TranslateEnv &env) const
{
	LabelOperand *lfalse, *ljoin;
	lfalse = env.newlabel();
	cond->translate(env, lfalse, true);
	if (sf)
		ljoin = env.newlabel();
	st->translate(env);
	if (sf)
		env.quads.emplace_back(Quad::JMP, ljoin);
	env.quads.emplace_back(Quad::LABEL, lfalse);
	if (sf) {
		sf->translate(env);
		env.quads.emplace_back(Quad::LABEL, ljoin);
	}
}

void WhileStmt::translate(TranslateEnv &env) const
{
	LabelOperand *lstart = env.newlabel();
	LabelOperand *lend = env.newlabel();
	env.quads.emplace_back(Quad::LABEL, lstart);
	cond->translate(env, lend, true);
	body->translate(env);
	env.quads.emplace_back(Quad::JMP, lstart);
	env.quads.emplace_back(Quad::LABEL, lend);
}

void DoWhileStmt::translate(TranslateEnv &env) const
{
	LabelOperand *lstart = env.newlabel();
	env.quads.emplace_back(Quad::LABEL, lstart);
	body->translate(env);
	cond->translate(env, lstart, false);
}

void ForStmt::translate(TranslateEnv &env) const
{
	// indvar = from;
	Operand *o_indvar = indvar->translate(env);
	env.quads.emplace_back(Quad::MOV, o_indvar, from->translate(env));
	// lim = to;
	TempOperand *lim = env.newtemp(indvar->type->size());
	env.quads.emplace_back(Quad::MOV, lim, to->translate(env));
	// while (indvar <= lim) {
	// 	<stmt>;
	// 	indvar++;
	// }
	LabelOperand *lstart = env.newlabel();
	LabelOperand *lend = env.newlabel();
	env.quads.emplace_back(Quad::LABEL, lstart);
	env.quads.emplace_back(down ? Quad::BLT : Quad::BGT, lend, o_indvar, lim);
	// <cond>
	body->translate(env);
	env.quads.emplace_back(down ? Quad::DEC : Quad::INC, o_indvar);
	env.quads.emplace_back(Quad::JMP, lstart);
	env.quads.emplace_back(Quad::LABEL, lend);
}

void ReadStmt::translate(TranslateEnv &env) const
{
	static LabelOperand o_scanf(EP "scanf");
	for (const unique_ptr<Expr> &var: vars) {
		Operand *o = var->translate(env);
		assert(o->kind == Operand::MEM);
		// nasm does not allow size prefix here
		static_cast<MemOperand*>(o)->size = 0;
		Operand *addr = env.newtemp(4);
		env.quads.emplace_back(Quad::LEA, addr, o);
		Operand *fmtstr;
		if (var->type == int_type())
			fmtstr = new LabelOperand("_$fmtsd");
		else if (var->type == char_type())
			fmtstr = new LabelOperand("_$fmtsc");
		else
			assert(0);
		env.quads.emplace_back(Quad::PUSH, addr);
		env.quads.emplace_back(Quad::PUSH, fmtstr);
		env.quads.emplace_back(Quad::CALL, &o_scanf);
		env.quads.emplace_back(Quad::ADD, esp, new ImmOperand(8));
	}
}

static vector<string> strings;

void WriteStmt::translate(TranslateEnv &env) const
{
	static LabelOperand o_printf(EP "printf");
	static LabelOperand o_putchar(EP "putchar");
	Operand *fmtstr;
	if (!str.empty()) {
		char strlabel[16];
		sprintf(strlabel, "_$s%d", int(strings.size()));
		strings.push_back(str);
		fmtstr = new LabelOperand("_$fmtps");
		env.quads.emplace_back(Quad::PUSH, new LabelOperand(strlabel));
		env.quads.emplace_back(Quad::PUSH, fmtstr);
		env.quads.emplace_back(Quad::CALL, &o_printf);
		env.quads.emplace_back(Quad::ADD, esp, new ImmOperand(8));
	}
	if (val) {
		if (val->type == int_type())
			fmtstr = new LabelOperand("_$fmtpd");
		else if (val->type == char_type())
			fmtstr = new LabelOperand("_$fmtpc");
		else
			assert(0);
		env.quads.emplace_back(Quad::PUSH, resize(env, 4, val->translate(env)));
		env.quads.emplace_back(Quad::PUSH, fmtstr);
		env.quads.emplace_back(Quad::CALL, &o_printf);
		env.quads.emplace_back(Quad::ADD, esp, new ImmOperand(8));
	}
	if (!str.empty() || val->type == int_type()) {
		env.quads.emplace_back(Quad::PUSH, new ImmOperand(10));
		env.quads.emplace_back(Quad::CALL, &o_putchar);
		env.quads.emplace_back(Quad::ADD, esp, new ImmOperand(4));
	}
}

void SimpleCond::translate(TranslateEnv &env, LabelOperand *label, bool negate) const
{
	Quad::Op qop;
	switch (op^negate) {
	case EQ: qop = Quad::BEQ; break;
	case NE: qop = Quad::BNE; break;
	case LT: qop = Quad::BLT; break;
	case GE: qop = Quad::BGE; break;
	case GT: qop = Quad::BGT; break;
	case LE: qop = Quad::BLE; break;
	default: assert(0);
	}
	env.quads.emplace_back(qop, label,
			       left ->translate(env),
			       right->translate(env));
}

void CompCond::translate(TranslateEnv &env, LabelOperand *ltrue, bool negate) const
{
	LabelOperand *lfalse;
	switch (op) {
	case AND:
		if (negate) {
			left ->translate(env, ltrue, true);
			right->translate(env, ltrue, true);
		} else {
			lfalse = env.newlabel();
			left ->translate(env, lfalse, true);
			right->translate(env, ltrue, false);
			env.quads.emplace_back(Quad::LABEL, lfalse);
		}
		break;
	case OR:
		if (negate) {
			lfalse = env.newlabel();
			left ->translate(env, lfalse, false);
			right->translate(env, ltrue, true);
			env.quads.emplace_back(Quad::LABEL, lfalse);
		} else {
			left ->translate(env, ltrue, false);
			right->translate(env, ltrue, false);
		}
		break;
	default: assert(0);
	}
}

void NegCond::translate(TranslateEnv &env, LabelOperand *ltrue, bool negate) const
{
	sub->translate(env, ltrue, !negate);
}

void TranslateEnv::allocaddr()
{
	int offset = 0;
	for (VarSymbol *vs: vars) {
		int size = vs->type->size();
		int align = vs->type->align();
		offset = (offset-size) & ~(align-1);
		vs->offset = offset;
	}
	//offset &= ~3;
	framesize = -offset;
}

void TranslateEnv::assign_scalar_id()
{
	int scalar_id = 0;
	for (VarSymbol *vs: vars) {
		if (vs->type->is_scalar()) {
			vs->scalar_id = scalar_id++;
			scalar_temp.push_back(newtemp(vs->type->size()));
		}
	}
}

void Block::translate(FILE *outfp)
{
	const char *block_name = proc ? proc->decorated_name.c_str() : EP "main";
	//printf("begin %s\n", block_name);
	//TranslateEnv env(symtab, block_name, outfp, framesize);
	TranslateEnv env(symtab, block_name, outfp, vars);
	if (proc)
		env.allocaddr();
	// else do nothing; main() has no local vars
	//env.assign_scalar_id();
	for (const unique_ptr<Block> &sub: subs)
		sub->translate(outfp);
	for (const unique_ptr<Stmt> &stmt: stmts)
		stmt->translate(env);
	if (proc) {
		Symbol *retval = symtab->lookup(proc->name+'$');
		if (retval) {
			assert(retval->kind == Symbol::VAR);
			VarSymbol *v = static_cast<VarSymbol*>(retval);
			int rvsize = v->type->size();
			env.quads.emplace_back(Quad::MOV, getphysreg(rvsize, 0), resize(env, rvsize, env.translate_sym(retval)));
		}
	}
	env.gencode();
	//printf("end %s\n", block_name);
}

static char sizechar(int size)
{
	switch (size) {
	case 4:
		return 'd';
	case 1:
		return 'b';
	}
	assert(0);
}

void translate_all(unique_ptr<Block> &&blk, const TranslateOptions &options)
{
	FILE *outfp = fopen("out.s", "w");
	fputs("\tglobal\t" EP "main\n"
	      "\textern\t" EP "printf, " EP "scanf, " EP "putchar\n"
	      "\n"
	      "\tsection\t.bss\n", outfp);
	for (VarSymbol *vs: blk->vars) {
		Type *type = vs->type;
		int align = type->align();
		fprintf(outfp, "$%s:\n", vs->name.c_str());
		fprintf(outfp, "\tres%c\t%d\n", sizechar(align), type->size()/align);
	}
	fputs("\n\tsection\t.text\n", outfp);
	blk->translate(outfp);
	//blk->print(0);
	fputs("\n"
	      "\tsection\t.data\n"
	      "_$fmtsd:\n"
	      "\tdb\t'%d',0\n"
	      "_$fmtsc:\n"
	      "\tdb\t' %c',0\n"
	      "_$fmtpd:\n"
	      "\tdb\t'%d',0\n"
	      "_$fmtpc:\n"
	      "\tdb\t'%c',0\n"
	      "_$fmtps:\n"
	      "\tdb\t'%s',0\n", outfp);
	int n = strings.size();
	for (int i=0; i<n; i++) {
		// TODO escape quotes
		fprintf(outfp, "_$s%d:\n\tdb\t\"%s\",0\n", i, strings[i].c_str());
	}
	fclose(outfp);
}

void TranslateEnv::rewrite_mem(MemOperand *m)
{
	auto check = [this](Operand *&o) {
		if (o->ismem()) {
			MemOperand *m = static_cast<MemOperand*>(o);
			rewrite_mem(m);
			assert(m->size == 4);
			TempOperand *t = newtemp(4);
			quads.emplace_back(Quad::MOV, t, m);
			o = t;
		}
	};
	check(m->base);
	check(m->index);
};

// eliminate invalid combination of opcode and operands, such as
//   mov     MEM, MEM
//   idiv    IMM
void TranslateEnv::rewrite()
{
	vector<Quad> oldquads = move(quads);
	quads.clear();
	// no const here, we may modify q
	for (Quad &q: oldquads) {
		switch (q.op) {
		case Quad::ADD:
		case Quad::SUB:
		case Quad::MUL:
		case Quad::MOV:
			// op c,a
			assert(q.c->istemp() || q.c->ismem());
			if (q.c->ismem() && q.a->ismem())
				q.a = totemp(q.a);
			break;
		case Quad::DIV:
		case Quad::NEG:
		case Quad::INC:
		case Quad::DEC:
			// op c
			// c can't be IMM
			if (q.c->isimm())
				q.c = totemp(q.c);
			break;
		case Quad::BEQ:
		case Quad::BNE:
		case Quad::BLT:
		case Quad::BGE:
		case Quad::BGT:
		case Quad::BLE:
			// bcc c,a,b
			assert(q.c->islabel());
			if (q.a->ismem() && q.b->ismem())
				q.a = totemp(q.a);
			break;
		case Quad::SEX:
			// movsx c,a
			assert(q.c->istemp());
			assert(q.a->istemp() || q.a->ismem());
			break;
		case Quad::JMP:
		case Quad::CALL:
			assert(q.c->islabel());
			break;
		case Quad::LEA:
			// lea c,a
			assert(q.c->istemp());
			assert(q.a->ismem());
			break;
		case Quad::PUSH:
			// push c
			// c can be anything, including LABEL
			break;
		case Quad::CDQ:
		case Quad::LABEL:
			// nullary
			break;
		default:
			assert(0);
		}
		assert(!(q.op == Quad::MOV && q.c->ismem() && q.a->ismem()));
		quads.emplace_back(q);
	}
}
