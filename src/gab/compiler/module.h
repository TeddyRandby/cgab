#ifndef BLUF_MODULE_H
#define BLUF_MODULE_H
#include "../../common/common.h"
#include "../vm/gc.h"
#include "object.h"
typedef struct gab_vm gab_vm;
typedef struct gab_engine gab_engine;
typedef struct gab_module gab_module;

/*
  State required to run a gab program.
  This is the result of compilation.
*/
struct gab_module {
  s_u8_ref name;

  u8 ntop_level;

  /*
    The instructions, a contiguous vector of single-byte op-codes and args.
  */
  v_u8 bytecode;

  /* A sister vector to 'bytecode'.
     This vector relates each instruction to a line in the source code.
  */
  v_u64 lines;

  /*
    A sister vector to 'bytecode'.
    This vector relates each instruction to a token.
  */
  v_u8 tokens;

  /*
     A vector of each line of source code.
  */
  v_s_u8_ref *source_lines;

  s_u8 *source;

  /*
    The running engine.
  */
  gab_engine *engine;

  /*
    The next module in the linked list of modules.
  */
  gab_module *next;
};

struct gab_engine {
  /*
    The constant table.
  */
  d_u64 constants;

  /*
    The std lib object.
  */
  gab_value std;

  /*
    A pointer to the vm running this module.
  */
  gab_vm *vm;

  /*
    The gargabe collector for the engine
  */
  gab_gc *gc;

  /*
    A linked list of loaded modules.
  */
  gab_module *modules;
};

gab_engine *gab_engine_create();
void gab_engine_destroy(gab_engine *self);

gab_obj_string *gab_engine_find_string(gab_engine *self, s_u8_ref str,
                                       u64 hash);
gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size,
                                     gab_value values[size], u64 stride,
                                     u64 hash);

u16 gab_engine_add_constant(gab_engine *self, gab_value value);

void gab_engine_add_module(gab_engine *self, gab_module *mod);

/*
  Creating and destroying modules, from nothing and from a base module.
*/
gab_module *gab_module_create(s_u8_ref name, s_u8 *source);
void gab_module_destroy(gab_module *self);
void gab_module_push_byte(gab_module *self, u8 b);

/*
  A debug function for printing the instructions in a module.

  Defined in common/log.c
*/
void gab_module_dump(gab_module *self, s_u8_ref name);
#endif
