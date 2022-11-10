#ifndef QSYM_COMMON_H_
#define QSYM_COMMON_H_

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <sys/mman.h>

#include "pin.H"
#include "logging.h"
#include "compiler.h"
#include "allocation.h"
#include <llvm/ADT/APSInt.h>

#define EXPR_COMPLEX_LEVEL_THRESHOLD 4
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

#endif
