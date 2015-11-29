#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

Type::~Type() {}
Type::Type(Kind kind): kind(kind) {}
IntType::IntType(): Type(INT) {}
CharType::CharType(): Type(CHAR) {}
ArrayType::ArrayType(Type *elemtype, int nelem): Type(ARRAY), elemtype(elemtype), nelem(nelem) {}

void IntType::print() const
{
	printf("int");
}

int IntType::size() const { return 4; }
int IntType::align()const { return 4; }

void CharType::print() const
{
	printf("char");
}

int CharType::size() const { return 1; }
int CharType::align()const { return 1; }

void ArrayType::print() const
{
	printf("array[%d] of ", nelem);
	elemtype->print();
}

int ArrayType::size() const
{
	return elemtype->size()*nelem;
}
int ArrayType::align() const
{
	return elemtype->align();
}

Type *binexprtype(Type *a, Type *b)
{
	if (a == char_type() && b == char_type())
		return char_type();
	if ((a == char_type() || a == int_type()) &&
	    (b == char_type() || b == int_type()))
		return int_type();
	return nullptr;//error_type();
}

Type *elemtype(Type *ty)
{
	if (ty->kind == Type::ARRAY)
		return static_cast<ArrayType*>(ty)->elemtype;
	return nullptr;
}
