#include <map>
#include <memory>
#include <string>
#include <vector>
#include "semant.h"

using namespace std;

void translate(unique_ptr<Block> &&blk)
{
	blk->print(0);
}
