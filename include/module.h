#ifndef GAB_MODULE_H
#define GAB_MODULE_H
#include "include/gab.h"
#include "lexer.h"

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

#define NAME gab_constant
#define T gab_value
#include "include/vector.h"

;

/*
  State required to run a gab program.
*/
struct gab_module {
  gab_module *next;

  gab_source *source;

  /*
    The constant table.
  */
  v_gab_constant constants;

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
   * A vector which relates each instruction to a slice in the source.
   */
  v_s_i8 sources;

  /*
   * The name of the module
   **/
  u16 name;

  u8 previous_compiled_op;
};

/*
  Creating and destroying modules, from nothing and from a base module.
*/
gab_module *gab_module_create(gab_value name, gab_source *src,
                              gab_module *next);

gab_module *gab_module_copy(gab_engine *gab, gab_module *self,
                            gab_module *next);

void gab_module_destroy(gab_engine *gab, gab_gc* gc, gab_module *mod);

/*
  Helpers for pushing ops into the module.
*/
void gab_module_push_op(gab_module *, gab_opcode, gab_token, u64, s_i8);

void gab_module_push_byte(gab_module *, u8, gab_token, u64, s_i8);

void gab_module_push_short(gab_module *, u16, gab_token, u64, s_i8);

/* These helpers return the instruction they push. */
u8 gab_module_push_load_local(gab_module *, u8, gab_token, u64, s_i8);
u8 gab_module_push_store_local(gab_module *, u8, gab_token, u64, s_i8);
u8 gab_module_push_load_upvalue(gab_module *, u8, gab_token, u64, s_i8);
u8 gab_module_push_return(gab_module *, u8, boolean vse, gab_token, u64, s_i8);
u8 gab_module_push_yield(gab_module *, u8, boolean vse, gab_token, u64, s_i8);
u8 gab_module_push_tuple(gab_module *, u8, boolean vse, gab_token, u64, s_i8);
u8 gab_module_push_send(gab_module *mod, u8 have, u16 message, boolean vse,
                        gab_token, u64, s_i8);
u8 gab_module_push_pop(gab_module *, u8, gab_token, u64, s_i8);

void gab_module_push_inline_cache(gab_module *, gab_token, u64, s_i8);
u64 gab_module_push_iter(gab_module *self, u8 nlocals, u8 start, gab_token t,
                         u64 l, s_i8 s);
void gab_module_push_next(gab_module *self, u8 iter, gab_token t, u64 l,
                          s_i8 s);
u64 gab_module_push_loop(gab_module *);
u64 gab_module_push_jump(gab_module *, u8, gab_token, u64, s_i8);

void gab_module_patch_jump(gab_module *, u64);
void gab_module_patch_loop(gab_module *, u64, gab_token, u64, s_i8);
boolean gab_module_try_patch_vse(gab_module *, u8);

u16 gab_module_add_constant(gab_module *, gab_value);

void gab_module_dump(gab_module *, s_i8);
#endif
