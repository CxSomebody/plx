#include <cassert>
#include <string>
#include <vector>
#include "translate.h"

using namespace std;

void TranslateEnv::gencode()
{
	for (const Quad &q: quads) {
	}
}

string ImmOperand::gencode() const
{
	char tmp[12];
	sprintf(tmp, "%d", val);
	return string(tmp);
}

string TempOperand::gencode() const
{
	if (id < 0) {
	}
	// unallocated!
	assert(0);
}

string LabelOperand::gencode() const
{
	return label;
}

string MemOperand::gencode() const
{
	return "[]";
}

string ListOperand::gencode() const
{
	// invoking gencode on ListOperand does not make sense
	assert(0);
}
