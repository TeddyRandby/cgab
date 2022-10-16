#ifndef GAB_MODULE_H
#define GAB_MODULE_H
#include "lexer.h"
#include "gc.h"

typedef struct gab_module gab_module;

/*
  The bytecode of the vm.
*/
typedef enum gab_opcode {
#define OP_CODE(name) OP_##name,
#include "bytecode.h"
#undef OP_CODE
} gab_opcode;

static const char *gab_opcode_names[] = {
#define OP_CODE(name) #name,
#include "bytecode.h"
#undef OP_CODE
};


/*
  State required to run a gab program.
*/
struct gab_module {
  /*
    The name of the module
  */
  s_i8 name;

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
  v_s_i8 *source_lines;

  /*
     A slice over the source code.
  */
  s_i8 source;

  /*
    The running engine.
  */
  // gab_engine *engine;

  /*
    The next module in the linked list of modules.
  */
  gab_module *next;

  u8 previous_compiled_op;
};

/*
  Creating and destroying modules, from nothing and from a base module.
*/
gab_module *gab_module_create(gab_module *, s_i8, s_i8);

void gab_module_destroy(gab_module *);

/*
  Helpers for pushing ops into the module.
*/
void gab_module_push_op(gab_module *, gab_opcode, gab_token, u64);
void gab_module_push_byte(gab_module *, u8, gab_token, u64);

void gab_module_push_short(gab_module *, u16, gab_token, u64);

/* These helpers return the instruction they push. */
u8 gab_module_push_load_local(gab_module *, u8, gab_token, u64);
u8 gab_module_push_load_upvalue(gab_module *self, u8, gab_token, u64, boolean);
u8 gab_module_push_load_const_upvalue(gab_module *self, u8, gab_token, u64);
u8 gab_module_push_store_local(gab_module *, u8, gab_token, u64);
u8 gab_module_push_store_upvalue(gab_module *, u8, gab_token, u64);
u8 gab_module_push_return(gab_module *, u8, u8, gab_token, u64);
u8 gab_module_push_call(gab_module *, u8, u8, gab_token, u64);
u8 gab_module_push_pop(gab_module *, u8, gab_token, u64);

void gab_module_push_inline_cache(gab_module *, gab_token, u64);
void gab_module_push_loop(gab_module *, u64, gab_token, u64);
u64 gab_module_push_jump(gab_module *, u8, gab_token, u64);

void gab_module_patch_jump(gab_module *, u64);
void gab_module_try_patch_vse(gab_module *, u8);

/*
  A debug function for printing the instructions in a module.

  Defined in common/log.c
*/
void gab_module_dump(gab_engine* gab, gab_module *self, s_i8 name);
#endif
