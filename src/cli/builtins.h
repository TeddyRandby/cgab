#include "include/gab.h"

gab_value gab_lib_print(gab_engine *eng, u8 argc, gab_value argv[argc]);

gab_value gab_lib_panic(gab_engine *gab, u8 argc, gab_value argv[argc]);

typedef struct import import;

#define NAME import
#define K u64
#define V import *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

void imports_create();

void imports_destroy(gab_engine *gab);

gab_value gab_lib_require(gab_engine *gab, u8 argc, gab_value argv[argc]);
