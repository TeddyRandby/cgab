#include "include/gab.h"

void gab_lib_print(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]);

void gab_lib_panic(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]);

void gab_lib_require(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]);

void gab_setup_builtins(gab_engine* gab, const char* it);
