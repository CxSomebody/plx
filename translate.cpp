#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "semant.h"
#include "translate.h"
#include "dynbitset.h"

using namespace std;

static const char *opstr[] = {
	"+=",
	"-=",
	"*=",
	nullptr,
	"=",
	"!=",
	"<",
	">=",
	">",
	"<=",
};

TranslateEnv::TranslateEnv(SymbolTable *symtab,
			   const std::string &procname,
			   FILE *outfp,
			   const vector<VarSymbol*> &params,
			   const vector<VarSymbol*> &vars,
			   TranslateEnv *up,
			   const TranslateOptions *opt):
	symtab(symtab),
	procname(procname),
	outfp(outfp),
	params(params),
	vars(vars),
	up(up),
	opt(opt)
{
	level = symtab->level;
	if (level < 1)
		level = 1;
	if (opt->optimize) {
		scalar_id = (up ? up->scalar_id : 0);
	} else {
		scalar_id = 0;
	}
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

TempOperand *al = &physreg1[0];

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

std::string args_tostr(Operand **args)
{
	stringstream ss;
	ss << '(';
	bool sep = false;
	for (int i=0; args[i]; i++) {
		if (sep)
			ss << ',';
		ss << args[i]->tostr();
		sep = true;
	}
	ss << ')';
	return ss.str();
}

std::string Quad::tostr() const
{
	stringstream ss;
	switch (op) {
	case Quad::ADD:
	case Quad::SUB:
	case Quad::MULW:
		ss << c->tostr() << ' ' << opstr[op] << ' ' << a->tostr();
		break;
	case Quad::DIV:
		ss << "div " << c->tostr();
		break;
	case Quad::NEG:
		ss << "neg " << c->tostr();
		break;
	case Quad::NEG2:
		ss << c->tostr() << " = -" << a->tostr();
		break;
	case Quad::MOV:
		ss << c->tostr() << " = " << a->tostr();
		break;
	case Quad::BEQ:
	case Quad::BNE:
	case Quad::BLT:
	case Quad::BGE:
	case Quad::BGT:
	case Quad::BLE:
		ss << "goto " << c->tostr() << " if " << a->tostr()
			<< ' ' << opstr[op] << ' ' << b->tostr();
		break;
	case Quad::CALL:
		ss << "call " << c->tostr();
		break;
	case Quad::LABEL:
		ss << c->tostr() << ':';
		break;
	case Quad::JMP:
		ss << "goto " << c->tostr();
		break;
	case Quad::LEA:
		ss << c->tostr() << " = &" << a->tostr();
		break;
	case Quad::PUSH:
		ss << "push " << c->tostr();
		break;
	case Quad::INC:
		ss << "inc " << c->tostr();
		break;
	case Quad::DEC:
		ss << "dec " << c->tostr();
		break;
	case Quad::SEX:
		ss << c->tostr() << " = sex " << a->tostr();
		break;
	case Quad::CDQ:
		ss << "cdq";
		break;
	case Quad::MULB:
		ss << "mulb " << c->tostr();
		break;
	case Quad::ADD3:
	case Quad::SUB3:
	case Quad::MUL3:
	case Quad::DIV3:
		static char opchar3[4] = {
			'+', '-', '*', '/'
		};
		ss << c->tostr() << " = " << a->tostr() << ' ' << opchar3[op-Quad::ADD3] << ' ' << b->tostr();
		break;
	case Quad::PHI:
		ss << c->tostr() << " = φ" << args_tostr(args);
		break;
	case Quad::SYNCM:
		ss << "sync_mem" << args_tostr(args);
		break;
	case Quad::SYNCR:
		ss << "sync_reg" << args_tostr(args);
		break;
	default:
		assert(0);
	}
	return ss.str();
}

TempOperand *TranslateEnv::newtemp(int size)
{
	return newtemp_scalar(size, -1);
}

TempOperand *TranslateEnv::newtemp_scalar(int size, int scalar)
{
	assert(int(temps.size()) == tempid);
	assert(int(temp_scalar.size()) == tempid);
	if (size < 4)
		part_temps.push_back(tempid);
	TempOperand *t = new TempOperand(size, tempid++);
	temps.push_back(t);
	temp_scalar.push_back(scalar);
	return t;
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

Operand *TranslateEnv::resize(int size, Operand *o)
{
	if (size == o->size)
		return o;
	if (size > o->size) {
		TempOperand *t = newtemp(size);
		quads.emplace_back(Quad::SEX, t, o);
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
		{
			int id = astemp(o)->id;
			part_temps.push_back(id);
			TempOperand *t = new TempOperand(size, id);
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

MemOperand *TranslateEnv::translate_varsym(const VarSymbol *vs)
{
	MemOperand *m;
	int size = vs->type->size();
	if (vs->level) {
		// local var
		Operand *bp;
		if (vs->level == level) {
			bp = ebp;
		} else {
			bp = new MemOperand(4, ebp, 8+4*(level-vs->level-1));
		}
		m = new MemOperand(vs->isref ? 4 : size, bp, vs->offset);
		if (vs->isref) {
			// m is a pointer
			m = new MemOperand(size, m);
		}
	} else {
		// global var
		m = new MemOperand(size, new LabelOperand('$'+vs->name));
	}
	return m;
}

MemOperand *TranslateEnv::translate_lvalue(const Expr *e)
{
	Operand *o;
	const SymExpr *se;
	switch (e->kind) {
	case Expr::SYM:
		se = static_cast<const SymExpr*>(e);
		assert(se->sym->kind == Symbol::VAR);
		return translate_varsym(static_cast<const VarSymbol*>(se->sym));
	case Expr::INDEX:
		o = e->translate(*this);
		assert(o->ismem());
		return static_cast<MemOperand*>(o);
	default:
		assert(0);
	}
}

Operand *TranslateEnv::translate_sym(const Symbol *sym)
{
	if (sym->kind == Symbol::VAR) {
		const VarSymbol *vs = static_cast<const VarSymbol*>(sym);
		if (opt->optimize) {
			if (vs->type->is_scalar()) {
				int sid = vs->scalar_id;
				assert(sid >= 0 && sid < (int)scalar_temp.size());
				return scalar_temp[vs->scalar_id];
			}
		}
		return translate_varsym(vs);
	}
	if (sym->kind == Symbol::PROC) {
		// address of function
		return new LabelOperand(static_cast<const ProcSymbol*>(sym)->decorated_name);
	}
	assert(sym->kind == Symbol::CONST);
	return new ImmOperand(static_cast<const ConstSymbol*>(sym)->val);
}

void TranslateEnv::translate_call(ProcSymbol *proc, const vector<unique_ptr<Expr>> &args)
{
	dynbitset visible_scalars(scalar_id);
	int i=0;
	assert(proc->params.size() == args.size());
	for (auto it = args.rbegin(); it != args.rend(); it++) {
		Expr *arg = it->get();
		Operand *o;
		if (proc->params[i].byref) {
			if (opt->optimize) {
				if (arg->kind == Expr::SYM) {
					Symbol *sym = static_cast<SymExpr*>(arg)->sym;
					if (sym->kind == Symbol::VAR) {
						VarSymbol *varsym = static_cast<VarSymbol*>(sym);
						if (varsym->scalar_id >= 0) {
							visible_scalars.set(varsym->scalar_id);
						}
					}
				}
			}
			MemOperand *m = translate_lvalue(arg);
			TempOperand *addr = newtemp(4);
			m->size = 0;
			quads.emplace_back(Quad::LEA, addr, m);
			o = addr;
		} else {
			o = arg->translate(*this);
		}
		quads.emplace_back(Quad::PUSH, resize(4, o));
		i++;
	}
	assert(proc->level > 0 && proc->level <= level+1);
	for (int i=1; i<proc->level; i++)
		quads.emplace_back(Quad::PUSH, i == level ?
				   static_cast<Operand*>(ebp) :
				   static_cast<Operand*>(new MemOperand(4, ebp, 8+(i-1)*4)));
	//printf("proc %s level=%d\n", proc->name.c_str(), proc->level);
	int spinc = (args.size()+(proc->level-1))*4;
	Operand **synclist;
	if (opt->optimize) {
#if 0
		fprintf(stderr, "%s(%d) calls %s(%d)\n",
			procname.c_str(), level,
			proc->name.c_str(), proc->level);
#endif
		bool is_inner = proc->level > level;
		int n = is_inner ? scalar_id : up ? int(up->scalar_temp.size()) : int(scalar_temp.size());
		for (int i=0; i<n; i++)
			visible_scalars.set(i);
		vector<int> vec_visible_scalars = visible_scalars.to_vector();
		
		synclist = (Operand **) calloc(vec_visible_scalars.size()+1, sizeof *synclist);
		n = vec_visible_scalars.size();
		for (int i=0; i<n; i++)
			synclist[i] = scalar_temp[vec_visible_scalars[i]];
		quads.emplace_back(Quad::SYNCM, nullptr, synclist);
	}
	quads.emplace_back(Quad::CALL, translate_sym(proc));
	if (opt->optimize) {
		quads.emplace_back(Quad::SYNCR, nullptr, synclist);
	}
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
	c = env.newtemp(type->size());
	a = env.resize(c->size, left ->translate(env));
	b = env.resize(c->size, right->translate(env));
	Quad::Op qop;
	switch (op) {
	case ADD: qop = Quad::ADD3; break;
	case SUB: qop = Quad::SUB3; break;
	case MUL: qop = Quad::MUL3; break;
	case DIV: qop = Quad::DIV3; break;
	default: assert(0);
	}
	env.quads.emplace_back(qop, c, a, b);
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
	MemOperand *m_array = static_cast<MemOperand*>(c);
	assert(!m_array->index);
	m_array->size = type->size();
	Operand *oindex = env.resize(4, index->translate(env));
	if (oindex->kind == Operand::IMM) {
		m_array->offset += static_cast<ImmOperand*>(oindex)->val * scale;
	} else {
		m_array->index = oindex;
		m_array->scale = scale;
	}
	return c;
}

Operand *UnaryExpr::translate(TranslateEnv &env) const
{
	Operand *c = env.newtemp(type->size());
	Operand *a = sub->translate(env);
	assert(c->size == a->size);
	Quad::Op qop;
	switch (op) {
	case NEG: qop = Quad::NEG2; break;
	default: assert(0);
	}
	env.quads.emplace_back(qop, c, a);
	return c;
}

Operand *ApplyExpr::translate(TranslateEnv &env) const
{
	env.translate_call(func, args);
	int size = type->size();
	TempOperand *t = env.newtemp(size);
	env.quads.emplace_back(Quad::MOV, t, env.resize(size, eax));
	return t;
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
	oval = env.resize(ovar->size, oval);
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
	env.quads.emplace_back(down ? Quad::SUB3 : Quad::ADD3,
			       o_indvar, o_indvar, new ImmOperand(1));
	env.quads.emplace_back(Quad::JMP, lstart);
	env.quads.emplace_back(Quad::LABEL, lend);
}

void ReadStmt::translate(TranslateEnv &env) const
{
	static LabelOperand o_scanf(EP "scanf");
	for (const unique_ptr<Expr> &var: vars) {
		Expr *e = var.get();
		MemOperand *m = env.translate_lvalue(e);
		// nasm does not allow size prefix here
		m->size = 0;
		Operand *addr = env.newtemp(4);
		env.quads.emplace_back(Quad::LEA, addr, m);
		Operand *fmtstr;
		if (e->type == int_type())
			fmtstr = new LabelOperand("_$fmtsd");
		else if (e->type == char_type())
			fmtstr = new LabelOperand("_$fmtsc");
		else
			assert(0);
		env.quads.emplace_back(Quad::PUSH, addr);
		env.quads.emplace_back(Quad::PUSH, fmtstr);
		env.quads.emplace_back(Quad::CALL, &o_scanf);
		if (env.opt->optimize) {
			if (e->kind == Expr::SYM) {
				Symbol *s = static_cast<SymExpr*>(e)->sym;
				assert(s->kind == Symbol::VAR);
				int a = static_cast<VarSymbol*>(s)->scalar_id;
				assert(a >= 0);
				Operand **synclist = (Operand **) calloc(2, sizeof *synclist);
				synclist[0] = env.scalar_temp[a];
				env.quads.emplace_back(Quad::SYNCR, nullptr, synclist);
			}
		}
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
		env.quads.emplace_back(Quad::PUSH, env.resize(4, val->translate(env)));
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
#if 0
	fprintf(stderr, "assign scalar id: %s\n", procname.c_str());
#endif
	if (up) {
		scalar_temp = up->scalar_temp;
		scalar_var = up->scalar_var;
		scalar_mem = up->scalar_mem;
		temps = scalar_temp;
		tempid = scalar_id;
		for (int i=0; i<tempid; i++)
			temp_scalar.push_back(i);
	}
	auto do_var = [&](VarSymbol *vs) {
		if (vs->type->is_scalar()) {
			assert(scalar_id == (int)scalar_temp.size());
#if 0
			fprintf(stderr, "%s %d\n", vs->name.c_str(), scalar_id);
#endif
			int size = vs->type->size();
			scalar_temp.push_back(newtemp_scalar(size, scalar_id));
			scalar_var.push_back(vs);
			vs->scalar_id = scalar_id++;
		}
	};
	for_each(params.begin(), params.end(), do_var);
	for_each(vars.begin(), vars.end(), do_var);
}

void translate_block(const Block &blk, FILE *outfp, TranslateEnv *up, const TranslateOptions *opt)
{
	const char *block_name = blk.proc ? blk.proc->decorated_name.c_str() : EP "main";
	//printf("begin %s\n", block_name);
	//TranslateEnv env(symtab, block_name, outfp, framesize);
	TranslateEnv env(blk.symtab, block_name, outfp, blk.params, blk.vars, up, opt);
	if (blk.proc)
		env.allocaddr();
	// else do nothing; main() has no local vars
	if (opt->optimize)
		env.assign_scalar_id();
	for (const unique_ptr<Block> &sub: blk.subs)
		translate_block(*sub, outfp, &env, opt);
	if (opt->optimize)
		env.sync(Quad::SYNCR);
	for (const unique_ptr<Stmt> &stmt: blk.stmts)
		stmt->translate(env);
	if (opt->optimize)
		env.sync(Quad::SYNCM);
	if (blk.proc) {
		Symbol *retval = blk.symtab->lookup(blk.proc->name+'$');
		if (retval) {
			// load return value into eax
			assert(retval->kind == Symbol::VAR);
			VarSymbol *v = static_cast<VarSymbol*>(retval);
			int rvsize = v->type->size();
			env.quads.emplace_back(Quad::MOV, getphysreg(rvsize, 0), env.resize(rvsize, env.translate_sym(retval)));
		}
	}
	if (opt->optimize) {
		env.insert_sync();
		if (opt->optimize >= 2)
			env.optimize();
	}
	env.lower();
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

void translate_all(unique_ptr<Block> &&blk, const TranslateOptions *opt)
{
	const char *out_fname = opt->out_fname.empty() ? "out.s" : opt->out_fname.c_str();
	FILE *outfp = fopen(out_fname, "w");
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
	translate_block(*blk, outfp, nullptr, opt);
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
		if (o && o->ismem()) {
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

void TranslateEnv::try_rewrite_mem(Operand *o)
{
	if (o && o->ismem())
		rewrite_mem(asmem(o));
};

// eliminate invalid combination of opcode and operands, such as
//   mov     MEM, MEM
//   idiv    IMM
//   cmp     IMM, _
void TranslateEnv::rewrite()
{
	vector<Quad> oldquads = move(quads);
	quads.clear();
	// no const here, we may modify q
	for (Quad &q: oldquads) {
		if (q.op == Quad::MOV && q.c->tostr() == q.a->tostr())
			continue;
		try_rewrite_mem(q.c);
		try_rewrite_mem(q.a);
		try_rewrite_mem(q.b);
		switch (q.op) {
		case Quad::ADD:
		case Quad::SUB:
		case Quad::MULW:
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
			if (q.a->ismem() && q.b->ismem()) {
				q.a = totemp(q.a);
			} else if (q.a->isimm()) {
				if (q.b->isimm()) {
					q.a = totemp(q.a);
				} else {
					swap(q.a, q.b);
					if (q.op >= Quad::BLT)
						q.op = Quad::Op(q.op^1);
				}
			}
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
		case Quad::MULB:
			// imul c
			// c is r/m8
			assert(q.c->size == 1);
			if (q.c->isimm())
				q.c = totemp(q.c);
			break;
		case Quad::CDQ:
		case Quad::LABEL:
			// nullary
			break;
		default:
			assert(0);
		}
		quads.emplace_back(q);
	}
}

void TranslateEnv::sync(Quad::Op op)
{
	if (up) {
		int n = up->scalar_id;
		vector<int> byref_scalars;
		for (VarSymbol *vs: params) {
			assert(vs->scalar_id >= 0);
			byref_scalars.push_back(vs->scalar_id);
		}
		int m = byref_scalars.size();
		Operand **args = (Operand **) calloc(n+m+1, sizeof *args);
		for (int i=0; i<n; i++)
			args[i] = scalar_temp[i];
		for (int i=0; i<m; i++)
			args[n+i] = scalar_temp[byref_scalars[i]];
		quads.emplace_back(op, nullptr, args);
	}
}

void TranslateEnv::lower()
{
	vector<Quad> oldquads(move(quads));
	for (const Quad &q: oldquads) {
		switch (q.op) {
		case Quad::ADD3:
		case Quad::SUB3:
		case Quad::MUL3:
			if (q.op == Quad::MUL3 && q.c->size != 4) {
				// mov al,a
				// imul b
				// mov c,al
				assert(q.c->size == 1);
				quads.emplace_back(Quad::MOV, al, q.a);
				quads.emplace_back(Quad::MULB, q.b);
				quads.emplace_back(Quad::MOV, q.c, al);
			} else {
				quads.emplace_back(Quad::MOV, q.c, q.a);
				quads.emplace_back(Quad::Op(Quad::ADD+(q.op-Quad::ADD3)), q.c, q.b);
			}
			break;
		case Quad::DIV3:
			if (q.c->size == 4) {
				quads.emplace_back(Quad::MOV, eax, q.a);
				quads.emplace_back(Quad::CDQ);
				quads.emplace_back(Quad::DIV, q.b);
				quads.emplace_back(Quad::MOV, q.c, eax);
			} else {
				TODO("DIV with size(c) != 4");
			}
			break;
		case Quad::NEG2:
			quads.emplace_back(Quad::MOV, q.c, q.a);
			quads.emplace_back(Quad::NEG, q.c);
			break;
		default:
			quads.push_back(q);
		}
	}
}

void TranslateEnv::sync_mem(int a)
{
	quads.emplace_back(Quad::MOV, scalar_temp[a], translate_varsym(scalar_var[a]));
}

void TranslateEnv::sync_reg(int a)
{
	quads.emplace_back(Quad::MOV, translate_varsym(scalar_var[a]), scalar_temp[a]);
}

void TranslateEnv::dump_quads()
{
	int n = quads.size();
	for (int i=0; i<n; i++)
		fprintf(stderr, "[%d] %s\n", i, quads[i].tostr().c_str());
}
