#include "include/gab.h"


typedef struct import import;

#define NAME import
#define K s_i8
#define V import*
#define DEF_V NULL
#define HASH(a) (s_i8_hash(a))
#define EQUAL(a, b) (s_i8_match(a, b))
#include "include/dict.h"

void imports_create();
void imports_cleanup(gab_engine* gab);
void imports_destroy();

gab_value gab_lib_require(gab_engine *eng, gab_vm* vm, gab_value receiver, u8 argc, gab_value argv[argc]);
