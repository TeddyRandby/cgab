#ifndef gab_mod_H
#define gab_mod_H
#include "include/gab.h"
#include "lexer.h"

typedef struct gab_mod gab_mod;

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
struct gab_mod {
  gab_mod *next;

  gab_src *source;

  /*
   * The name of the module
   **/
  gab_value name;

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

  u8 previous_compiled_op;
};

/*
  Creating and destroying modules, from nothing and from a base module.
*/
gab_mod *gab_mod_create(gab_eg *gab, gab_value name, gab_src *src);

void gab_mod_destroy(gab_eg *gab, gab_gc *gc, gab_mod *mod);

gab_mod *gab_mod_copy(gab_eg *gab, gab_mod *self);

/*
  Helpers for pushing ops into the module.
*/
void gab_mod_push_op(gab_mod *, gab_opcode, gab_token, u64, s_i8);

void gab_mod_push_byte(gab_mod *, u8, gab_token, u64, s_i8);

void gab_mod_push_short(gab_mod *, u16, gab_token, u64, s_i8);

void gab_mod_push_load_local(gab_mod *, u8, gab_token, u64, s_i8);

void gab_mod_push_store_local(gab_mod *, u8, gab_token, u64, s_i8);

void gab_mod_push_load_upvalue(gab_mod *, u8, gab_token, u64, s_i8);

void gab_mod_push_return(gab_mod *, u8, boolean mv, gab_token, u64,
                            s_i8);

void gab_mod_push_tuple(gab_mod *, u8, boolean mv, gab_token, u64, s_i8);

void gab_mod_push_yield(gab_mod *, u16 proto, u8 have, boolean mv,
                           gab_token, u64, s_i8);

void gab_mod_push_pack(gab_mod *self, u8 below, u8 above, gab_token, u64, s_i8);

void gab_mod_push_send(gab_mod *mod, u8 have, u16 message, boolean mv,
                          gab_token, u64, s_i8);

void gab_mod_push_pop(gab_mod *, u8, gab_token, u64, s_i8);

void gab_mod_push_inline_cache(gab_mod *, gab_token, u64, s_i8);

u64 gab_mod_push_iter(gab_mod *self, u8 start, u8 want, gab_token t,
                         u64 l, s_i8 s);

void gab_mod_push_next(gab_mod *self, u8 local, gab_token t, u64 l,
                          s_i8 s);

u64 gab_mod_push_loop(gab_mod *gab);

u64 gab_mod_push_jump(gab_mod *gab, u8, gab_token, u64, s_i8);

void gab_mod_patch_jump(gab_mod *, u64);

void gab_mod_patch_loop(gab_mod *, u64, gab_token, u64, s_i8);

boolean gab_mod_try_patch_mv(gab_mod *, u8);

u16 gab_mod_add_constant(gab_mod *, gab_value);

void gab_mod_dump(gab_mod *, s_i8);
#endif
