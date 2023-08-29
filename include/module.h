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
  v_uint8_t bytecode;

  /* A sister vector to 'bytecode'.
     This vector relates each instruction to a line in the source code.
  */
  v_uint64_t lines;

  /*
    A sister vector to 'bytecode'.
    This vector relates each instruction to a token.
  */
  v_uint8_t tokens;

  /*
   * A vector which relates each instruction to a slice in the source.
   */
  v_s_int8_t sources;

  uint8_t previous_compiled_op;
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
void gab_mod_push_op(gab_mod *, gab_opcode, gab_token, uint64_t, s_int8_t);

void gab_mod_push_byte(gab_mod *, uint8_t, gab_token, uint64_t, s_int8_t);

void gab_mod_push_short(gab_mod *, uint16_t, gab_token, uint64_t, s_int8_t);

void gab_mod_push_load_local(gab_mod *, uint8_t, gab_token, uint64_t, s_int8_t);

void gab_mod_push_store_local(gab_mod *, uint8_t, gab_token, uint64_t, s_int8_t);

void gab_mod_push_load_upvalue(gab_mod *, uint8_t, gab_token, uint64_t, s_int8_t);

void gab_mod_push_return(gab_mod *, uint8_t, bool mv, gab_token, uint64_t,
                            s_int8_t);

void gab_mod_push_tuple(gab_mod *, uint8_t, bool mv, gab_token, uint64_t, s_int8_t);

void gab_mod_push_yield(gab_mod *, uint16_t proto, uint8_t have, bool mv,
                           gab_token, uint64_t, s_int8_t);

void gab_mod_push_pack(gab_mod *self, uint8_t below, uint8_t above, gab_token, uint64_t, s_int8_t);

void gab_mod_push_send(gab_mod *mod, uint8_t have, uint16_t message, bool mv,
                          gab_token, uint64_t, s_int8_t);

void gab_mod_push_pop(gab_mod *, uint8_t, gab_token, uint64_t, s_int8_t);

void gab_mod_push_inline_cache(gab_mod *, gab_token, uint64_t, s_int8_t);

uint64_t gab_mod_push_iter(gab_mod *self, uint8_t start, uint8_t want, gab_token t,
                         uint64_t l, s_int8_t s);

void gab_mod_push_next(gab_mod *self, uint8_t local, gab_token t, uint64_t l,
                          s_int8_t s);

uint64_t gab_mod_push_loop(gab_mod *gab);

uint64_t gab_mod_push_jump(gab_mod *gab, uint8_t, gab_token, uint64_t, s_int8_t);

void gab_mod_patch_jump(gab_mod *, uint64_t);

void gab_mod_patch_loop(gab_mod *, uint64_t, gab_token, uint64_t, s_int8_t);

bool gab_mod_try_patch_mv(gab_mod *, uint8_t);

uint16_t gab_mod_add_constant(gab_mod *, gab_value);

void gab_mod_dump(gab_mod *, s_int8_t);
#endif
