#include "include/gab.h"

gab_value gab_lib_print(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]);

gab_value gab_lib_panic(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]);

gab_value gab_lib_require(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]);
