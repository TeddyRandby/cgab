#include "include/gab.h"

gab_value gab_lib_print(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]);


typedef struct import import;

#define NAME import
#define K s_i8
#define V import*
#define DEF_V NULL
#define HASH(a) (s_i8_hash(a))
#define EQUAL(a, b) (s_i8_match(a, b))
#include "include/dict.h"

void imports_create();
void imports_destroy(gab_engine* gab);

gab_value gab_lib_require(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]);

gab_value gab_lib_panic(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]);
