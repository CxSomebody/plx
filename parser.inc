#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "lexer.h"
#include "tokens.h"
}

#include "semant.h"

using namespace std;

typedef Type *PType;
