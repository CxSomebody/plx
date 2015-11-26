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

void IntType::print()
{
	printf("int");
}

int IntType::size() { return 4; }
int IntType::align(){ return 4; }

void CharType::print()
{
	printf("char");
}

int CharType::size() { return 1; }
int CharType::align(){ return 1; }

void ArrayType::print()
{
	printf("array[%d] of ", nelem);
	elemtype->print();
}

int ArrayType::size()
{
	return elemtype->size()*nelem;
}
int ArrayType::align()
{
	return elemtype->align();
}
