#include "include/compiler.h"
#include "include/char.h"
#include "include/colors.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/lexer.h"
#include "include/module.h"
#include "include/object.h"
#include "include/value.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
};

/*
  Precedence rules for the parsing of expressions.
*/
typedef enum gab_precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or, infix else
  PREC_AND,         // and, infix then
  PREC_MATCH,       // match
  PREC_EQUALITY,    // ==
  PREC_COMPARISON,  // < > <= >=
  PREC_BITWISE_OR,  // |
  PREC_BITWISE_AND, // &
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! - not
  PREC_SEND,        // ( { :
  PREC_PROPERTY,    // .
  PREC_PRIMARY
} gab_precedence;

/*
  Compile rules used for Pratt parsing of expressions.
*/
typedef i32 (*gab_compile_func)(gab_engine *, gab_bc *, boolean);

typedef struct gab_compile_rule gab_compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  gab_precedence prec;
  boolean multi_line;
};

void gab_bc_create(gab_bc *self, gab_source *source, u8 flags) {
  memset(self, 0, sizeof(*self));

  self->flags = flags;
  gab_lexer_create(&self->lex, source);
}

// A positive result is known to be OK, and can carry a value.
enum comp_status {
  COMP_OK = 1,
  COMP_TOKEN_NO_MATCH = 0,
  COMP_ERR = -1,
  COMP_LOCAL_NOT_FOUND = -2,
  COMP_UPVALUE_NOT_FOUND = -3,
  COMP_ID_NOT_FOUND = -4,
  COMP_RESOLVED_TO_LOCAL = -5,
  COMP_RESOLVED_TO_UPVALUE = -6,
  COMP_CLOSURE = -7,
  COMP_METHOD = -8,
  COMP_MAX = INT32_MAX,
};

static void compiler_error(gab_bc *bc, gab_status e, const char *help_fmt, ...);

static boolean match_token(gab_bc *bc, gab_token tok);

static i32 eat_token(gab_bc *bc);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline boolean match_token(gab_bc *bc, gab_token tok) {
  return bc->current_token == tok;
}

static inline boolean match_terminator(gab_bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL);
}

// Returns less than 0 if there was an error, greater than 0 otherwise.
static i32 eat_token(gab_bc *bc) {
  bc->previous_token = bc->current_token;
  bc->current_token = gab_lexer_next(&bc->lex);
  bc->line = bc->lex.current_row;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    compiler_error(bc, bc->lex.status, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 match_and_eat_token(gab_bc *bc, gab_token tok) {
  if (!match_token(bc, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

static inline i32 expect_token(gab_bc *bc, gab_token tok) {
  if (!match_token(bc, tok)) {
    eat_token(bc);
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected %s",
                   gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(bc);
}

static inline i32 expect_oneof(gab_bc *bc, gab_token toka, gab_token tokb) {
  if (match_and_eat_token(bc, toka))
    return COMP_OK;

  return expect_token(bc, tokb);
}

s_i8 trim_prev_tok(gab_bc *bc) {
  s_i8 message = bc->lex.previous_token_src;
  // SKip the ':' at the beginning
  message.data++;
  message.len--;

  return message;
}

static inline gab_bc_frame *peek_frame(gab_bc *bc, i32 depth) {
  return &bc->frames[bc->nframe - depth - 1];
}

static inline gab_module *mod(gab_bc *bc) { return peek_frame(bc, 0)->mod; }

static inline u16 add_constant(gab_module *mod, gab_value value) {
  return gab_module_add_constant(mod, value);
}

static inline void push_op(gab_bc *bc, gab_opcode op) {
  gab_module_push_op(mod(bc), op, bc->previous_token, bc->line,
                     bc->lex.previous_token_src);
}

static inline void push_pop(gab_bc *bc, u8 n) {
  gab_module_push_pop(mod(bc), n, bc->previous_token, bc->line,
                      bc->lex.previous_token_src);
}

static inline void push_store_local(gab_bc *bc, u8 local) {
  gab_module_push_store_local(mod(bc), local, bc->previous_token, bc->line,
                              bc->lex.previous_token_src);
}

static inline void push_load_local(gab_bc *bc, u8 local) {
  gab_module_push_load_local(mod(bc), local, bc->previous_token, bc->line,
                             bc->lex.previous_token_src);
}

static inline void push_load_upvalue(gab_bc *bc, u8 upv) {
  gab_module_push_load_upvalue(mod(bc), upv, bc->previous_token, bc->line,
                               bc->lex.previous_token_src);
}

static inline void push_byte(gab_bc *bc, u8 data) {
  gab_module_push_byte(mod(bc), data, bc->previous_token, bc->line,
                       bc->lex.previous_token_src);
}

static inline void push_short(gab_bc *bc, u16 data) {
  gab_module_push_short(mod(bc), data, bc->previous_token, bc->line,
                        bc->lex.previous_token_src);
}

static inline i32 push_impl(gab_engine *gab, gab_bc *bc, u8 local) {

  if (bc->nimpls == UINT8_MAX) {
    compiler_error(bc, GAB_PANIC, "Too many nested impls");
    return COMP_ERR;
  }

  bc->impls[bc->nimpls++] = local;

  return COMP_OK;
}

static inline void pop_impl(gab_bc *bc) { bc->nimpls--; }

static inline boolean has_impl(gab_bc *bc) { return bc->nimpls > 0; }

static inline gab_value peek_impl(gab_bc *bc) {
  return bc->impls[bc->nimpls - 1];
}

static inline i32 push_slot(gab_bc *bc, u8 n) {
  gab_bc_frame *f = peek_frame(bc, 0);

  if (f->next_slot + n >= UINT16_MAX) {
    compiler_error(bc, GAB_PANIC, "Too many slots\n");
    return COMP_ERR;
  }

  f->next_slot += n;

  if (f->next_slot > f->nslots)
    f->nslots = f->next_slot;

  return COMP_OK;
}

static inline void pop_slot(gab_bc *bc, u8 n) {
  if (peek_frame(bc, 0)->next_slot - n < 0) {
    compiler_error(bc, GAB_PANIC, "Too few slots\n");
  }
  peek_frame(bc, 0)->next_slot -= n;
}

static void initialize_local(gab_bc *bc, u8 local) {
  gab_bc_frame *f = peek_frame(bc, 0);

  f->locals_depth[local] = f->scope_depth;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static i32 add_local(gab_engine *gab, gab_bc *bc, gab_value name, u8 flags) {
  gab_bc_frame *f = peek_frame(bc, 0);

  if (f->nlocals == LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  u8 local = f->next_local;
  f->locals_name[local] = add_constant(mod(bc), name);
  f->locals_depth[local] = -1;
  f->locals_flag[local] = flags;

  if (++f->next_local > f->nlocals)
    f->nlocals = f->next_local;

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * upvalue.
 */
static i32 add_upvalue(gab_bc *bc, u32 depth, u8 index, u8 flags) {
  gab_bc_frame *f = peek_frame(bc, depth);
  u16 count = f->nupvalues;

  // Don't pull redundant upvalues
  for (int i = 0; i < count; i++) {
    if (f->upvs_index[i] == index && (f->upvs_flag[i]) == flags) {
      return i;
    }
  }

  if (count == UPVALUE_MAX) {
    compiler_error(bc, GAB_TOO_MANY_UPVALUES, "");
    return COMP_ERR;
  }

  f->upvs_index[count] = index;
  f->upvs_flag[count] = flags;
  f->nupvalues++;

  return count;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_LOCAL_NOT_FOUND if no matching local is found,
 * and otherwise the offset of the local.
 */
static i32 resolve_local(gab_engine *gab, gab_bc *bc, gab_value name,
                         u32 depth) {
  gab_bc_frame *f = peek_frame(bc, depth);

  for (i32 local = f->next_local - 1; local >= 0; local--) {
    gab_value other_local_name =
        v_gab_constant_val_at(&f->mod->constants, f->locals_name[local]);

    if (name == other_local_name) {
      if (f->locals_depth[local] == -1) {
        compiler_error(bc, GAB_REFERENCE_BEFORE_INITIALIZE, "");
        return COMP_ERR;
      }
      return local;
    }
  }
  return COMP_LOCAL_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_UPVALUE_NOT_FOUND if no matching upvalue is found,
 * and otherwise the offset of the upvalue.
 */
static i32 resolve_upvalue(gab_engine *gab, gab_bc *bc, gab_value name,
                           u32 depth) {
  // Base case, hopefully conversion doesn't cause issues
  if (depth >= bc->nframe - 1) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  i32 local = resolve_local(gab, bc, name, depth + 1);
  if (local >= 0) {
    u8 flags = (peek_frame(bc, depth + 1)->locals_flag[local] |=
                GAB_VARIABLE_FLAG_CAPTURED);

    if (flags & GAB_VARIABLE_FLAG_MUTABLE) {
      compiler_error(bc, GAB_CAPTURED_MUTABLE, "Tried to capture %V", name);
      return COMP_ERR;
    }

    return add_upvalue(bc, depth, local, flags | GAB_VARIABLE_FLAG_LOCAL);
  }

  i32 upvalue = resolve_upvalue(gab, bc, name, depth + 1);
  if (upvalue >= 0) {
    u8 flags = peek_frame(bc, depth + 1)->upvs_flag[upvalue];
    return add_upvalue(bc, depth, upvalue, flags & ~GAB_VARIABLE_FLAG_LOCAL);
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_RESOLVED_TO_LOCAL if the id was a local, and
 * COMP_RESOLVED_TO_UPVALUE if the id was an upvalue.
 */
static i32 resolve_id(gab_engine *gab, gab_bc *bc, gab_value name,
                      u8 *value_in) {

  i32 arg = resolve_local(gab, bc, name, 0);

  if (arg == COMP_ERR)
    return arg;

  if (arg == COMP_LOCAL_NOT_FOUND) {

    arg = resolve_upvalue(gab, bc, name, 0);
    if (arg == COMP_ERR)
      return arg;

    if (arg == COMP_UPVALUE_NOT_FOUND)
      return COMP_ID_NOT_FOUND;

    if (value_in)
      *value_in = (u8)arg;
    return COMP_RESOLVED_TO_UPVALUE;
  }

  if (value_in)
    *value_in = (u8)arg;
  return COMP_RESOLVED_TO_LOCAL;
}

static inline i32 push_load_id(gab_engine *gab, gab_bc *bc, gab_value name) {
  u8 value;
  switch (resolve_id(gab, bc, name, &value)) {
  case COMP_RESOLVED_TO_LOCAL:
    push_load_local(bc, value);
    return COMP_OK;
  case COMP_RESOLVED_TO_UPVALUE:
    push_load_upvalue(bc, value);
    return COMP_OK;
  default:
    compiler_error(bc, GAB_MISSING_IDENTIFIER, "");
    return COMP_ERR;
  }
}

static void down_scope(gab_bc *bc) { peek_frame(bc, 0)->scope_depth++; }

static void up_scope(gab_bc *bc) {
  gab_bc_frame *f = peek_frame(bc, 0);

  f->scope_depth--;

  while (f->next_local > 1 &&
         f->locals_depth[f->next_local - 1] > f->scope_depth) {
    f->next_local--;
    pop_slot(bc, 1);
  }
}

static gab_module *down_frame(gab_engine *gab, gab_bc *bc, gab_value name,
                              boolean is_method) {
  gab_bc_frame *f = bc->frames + bc->nframe++;

  gab_module *mod = gab_module_create(name, bc->lex.source, gab->modules);
  gab->modules = mod;

  f->mod = mod;

  f->next_local = 0;
  f->nlocals = 0;

  f->next_slot = 0;
  f->nslots = 0;

  f->nupvalues = 0;
  f->scope_depth = 0;

  initialize_local(bc, add_local(gab, bc, GAB_STRING("self"), 0));

  return mod;
}

static gab_obj_prototype *up_frame(gab_engine *gab, gab_bc *bc, u8 nargs,
                                   boolean vse) {
  gab_bc_frame *frame = peek_frame(bc, 0);

  u8 nupvalues = frame->nupvalues;
  u8 nslots = frame->nslots;
  u8 nlocals = frame->nlocals;

  gab_obj_prototype *p = gab_obj_prototype_create(
      gab, frame->mod, nargs, nslots, nupvalues, nlocals, vse, frame->upvs_flag,
      frame->upvs_index);

  gab_obj_block *main = gab_obj_block_create(gab, p);

  frame->mod->main = add_constant(mod(bc), GAB_VAL_OBJ(main));

  bc->nframe--;

  return p;
}

gab_compile_rule get_rule(gab_token k);
i32 compile_exp_prec(gab_engine *gab, gab_bc *bc, gab_precedence prec);
i32 compile_expression(gab_engine *gab, gab_bc *bc);
i32 compile_tuple(gab_engine *gab, gab_bc *bc, u8 want, boolean *vse_out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

static i32 compile_local(gab_engine *gab, gab_bc *bc, gab_value name,
                         u8 flags) {
  gab_bc_frame *f = peek_frame(bc, 0);

  for (i32 local = f->next_local - 1; local >= 0; local--) {
    if (f->locals_depth[local] != -1 &&
        f->locals_depth[local] < f->scope_depth) {
      break;
    }

    gab_value other_local_name =
        v_gab_constant_val_at(&f->mod->constants, f->locals_name[local]);

    if (name == other_local_name) {
      compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }
  }

  return add_local(gab, bc, name, flags);
}

i32 compile_parameters(gab_engine *gab, gab_bc *bc, boolean *vse_out) {
  boolean vse = false;
  i32 result = 0;
  u8 narguments = 0;

  if (!match_and_eat_token(bc, TOKEN_LPAREN)) {
    goto fin;
  }

  if (match_and_eat_token(bc, TOKEN_RPAREN) > 0) {
    goto fin;
  }

  do {

    if (narguments >= ARG_MAX) {
      compiler_error(bc, GAB_TOO_MANY_PARAMETERS, "");
      return COMP_ERR;
    }

    narguments++;

    switch (match_and_eat_token(bc, TOKEN_DOT_DOT)) {
    case COMP_OK: {
      // This is a vararg parameter
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      s_i8 name = bc->lex.previous_token_src;

      gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

      i32 local = compile_local(gab, bc, val_name, 0);

      if (local < 0)
        return COMP_ERR;

      initialize_local(bc, local);

      if (expect_token(bc, TOKEN_RPAREN) < 0)
        return COMP_ERR;

      // This should be the last parameter
      vse = true;
      narguments--;
      goto fin;
    }

    case COMP_TOKEN_NO_MATCH: {
      // This is a normal paramter
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      s_i8 name = bc->lex.previous_token_src;

      gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

      i32 local = compile_local(gab, bc, val_name, 0);

      if (local < 0)
        return COMP_ERR;

      initialize_local(bc, local);

      break;
    }

    default:
      compiler_error(bc, GAB_UNEXPECTED_TOKEN, "While compiling parameter");
      return COMP_ERR;
    }

  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)) > 0);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

fin:
  if (vse_out)
    *vse_out = vse;

  return narguments;
}

static inline i32 skip_newlines(gab_bc *bc) {
  while (match_token(bc, TOKEN_NEWLINE)) {
    if (eat_token(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 optional_newline(gab_bc *bc) {
  match_and_eat_token(bc, TOKEN_NEWLINE);
  return COMP_OK;
}

i32 compile_expressions_body(gab_engine *gab, gab_bc *bc) {
  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (match_terminator(bc))
    return COMP_OK;

  if (expect_oneof(bc, TOKEN_SEMICOLON, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_terminator(bc) && !match_token(bc, TOKEN_EOF)) {
    push_pop(bc, 1);

    pop_slot(bc, 1);

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    if (match_terminator(bc))
      return COMP_OK;

    if (expect_oneof(bc, TOKEN_SEMICOLON, TOKEN_NEWLINE) < 0)
      return COMP_ERR;

    if (skip_newlines(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_expressions(gab_engine *gab, gab_bc *bc) {
  down_scope(bc);

  if (compile_expressions_body(gab, bc) < 0)
    return COMP_ERR;

  up_scope(bc);

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    compiler_error(bc, GAB_MISSING_END, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_block_body(gab_engine *gab, gab_bc *bc, u8 narguments, u8 vse) {
  i32 result = compile_expressions(gab, bc);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_module_push_return(mod(bc), result, false, bc->previous_token, bc->line,
                         bc->lex.previous_token_src);

  return COMP_OK;
}

i32 compile_message_spec(gab_engine *gab, gab_bc *bc) {
  if (match_and_eat_token(bc, TOKEN_LBRACE)) {

    if (match_and_eat_token(bc, TOKEN_RBRACE)) {
      if (push_slot(bc, 1) < 0)
        return COMP_ERR;

      push_op(bc, OP_PUSH_UNDEFINED);

      return COMP_OK;
    }

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    return COMP_OK;
  }

  if (has_impl(bc)) {
    push_load_local(bc, peek_impl(bc));
    return COMP_OK;
  }

  compiler_error(bc, GAB_MISSING_RECEIVER, "");
  return COMP_ERR;
}

i32 compile_block(gab_engine *gab, gab_bc *bc) {
  down_scope(bc);

  down_frame(gab, bc, GAB_STRING("anonymous"), false);

  boolean vse;
  i32 narguments = compile_parameters(gab, bc, &vse);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc, narguments, vse) < 0)
    return COMP_ERR;

  gab_obj_prototype *p = up_frame(gab, bc, narguments, vse);

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_BLOCK);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(p)));

  push_slot(bc, 1);
  return COMP_OK;
}

i32 compile_message(gab_engine *gab, gab_bc *bc, gab_value name) {
  if (compile_message_spec(gab, bc) < 0)
    return COMP_ERR;

  down_scope(bc);

  down_frame(gab, bc, name, true);

  boolean vse;
  i32 narguments = compile_parameters(gab, bc, &vse);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc, narguments, vse) < 0)
    return COMP_ERR;

  gab_obj_prototype *p = up_frame(gab, bc, narguments, vse);

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_MESSAGE);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(p)));

  gab_obj_message *m = gab_obj_message_create(gab, name);
  u16 func_constant = add_constant(mod(bc), GAB_VAL_OBJ(m));
  push_short(bc, func_constant);

  push_slot(bc, 1);
  return COMP_OK;
}

i32 compile_expression(gab_engine *gab, gab_bc *bc) {
  return compile_exp_prec(gab, bc, PREC_ASSIGNMENT);
}

i32 compile_tuple(gab_engine *gab, gab_bc *bc, u8 want, boolean *vse_out) {
  u8 have = 0;
  boolean vse;

  i32 result;
  do {
    if (optional_newline(bc) < 0)
      return COMP_ERR;

    vse = false;
    result = compile_exp_prec(gab, bc, PREC_ASSIGNMENT);

    if (result < 0)
      return COMP_ERR;

    vse = result == VAR_EXP;
    have++;
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)) == COMP_OK);

  if (result < 0)
    return COMP_ERR;

  if (have > want) {
    compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS, "");
    return COMP_ERR;
  }

  if (want == VAR_EXP) {
    /*
     * If we want all possible values, try to patch a VSE.
     * If we are successful, remove one from have.
     * This is because have's meaning changes to mean the number of
     * values in ADDITION to the VSE ending the tuple.
     */
    have -= gab_module_try_patch_vse(mod(bc), VAR_EXP);
  } else {
    /*
     * Here we want a specific number of values. Try to patch the VSE to want
     * however many values we need in order to match up have and want. Again, we
     * subtract an extra one because in the case where we do patch, have's
     * meaning is now the number of ADDITIONAL values we have.
     */
    if (gab_module_try_patch_vse(mod(bc), want - have + 1)) {
      // If we were successful, we have all the values we want.
      push_slot(bc, want - have + 1);
      have = want;
    }

    /*
     * If we failed to patch and still don't have enough, push some Nils.
     */
    while (have < want) {
      // While we have fewer expressions than we want, push nulls.
      push_op(bc, OP_PUSH_NIL);
      have++;
    }
  }

  if (vse_out)
    *vse_out = vse;
  // If we have an out argument, set its values.
  return have;
}

i32 add_message_constant(gab_engine *gab, gab_module *mod, gab_value name) {
  gab_obj_message *f = gab_obj_message_create(gab, name);

  return add_constant(mod, GAB_VAL_OBJ(f));
}

i32 add_string_constant(gab_engine *gab, gab_module *mod, s_i8 str) {
  gab_obj_string *obj = gab_obj_string_create(gab, str);

  return add_constant(mod, GAB_VAL_OBJ(obj));
}

i32 compile_property(gab_engine *gab, gab_bc *bc, boolean assignable) {
  s_i8 prop_name = trim_prev_tok(bc);

  i32 prop = add_string_constant(gab, mod(bc), prop_name);

  gab_token prop_tok = bc->previous_token;
  u64 prop_line = bc->line;
  s_i8 prop_src = bc->lex.previous_token_src;

  if (prop < 0)
    return COMP_ERR;

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
      return COMP_ERR;
    }

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    gab_module_push_op(mod(bc), OP_STORE_PROPERTY_ANA, prop_tok, prop_line,
                       prop_src);

    gab_module_push_short(mod(bc), prop, prop_tok, prop_line, prop_src);

    gab_module_push_inline_cache(mod(bc), prop_tok, prop_line, prop_src);

    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    gab_module_push_op(mod(bc), OP_LOAD_PROPERTY_ANA, prop_tok, prop_line,
                       prop_src);

    gab_module_push_short(mod(bc), prop, prop_tok, prop_line, prop_src);

    gab_module_push_inline_cache(mod(bc), prop_tok, prop_line, prop_src);
    break;
  }

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "While compiling property access");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_lst_internal_item(gab_engine *gab, gab_bc *bc, u8 index) {
  return compile_expression(gab, bc);
}

i32 compile_lst_internals(gab_engine *gab, gab_bc *bc, boolean *vse_out) {
  u8 size = 0;
  boolean vse = false;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_token(bc, TOKEN_RBRACE)) {

    i32 result = compile_lst_internal_item(gab, bc, size);

    vse = result == VAR_EXP;

    if (result < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(bc) < 0)
      return COMP_ERR;
    size++;
  }

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  if (vse_out)
    *vse_out = vse;

  if (vse) {
    return size - gab_module_try_patch_vse(mod(bc), VAR_EXP);
  }

  return size;
}

i32 compile_var_decl(gab_engine *gab, gab_bc *bc, u8 flags,
                     u8 additional_local) {
  u8 locals[16] = {additional_local};

  u8 local_count = additional_local > 0;

  i32 result = COMP_OK;

  if (match_token(bc, TOKEN_EQUAL))
    goto initializer;

  do {
    if (local_count == 16) {
      compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_LET, "");
      return COMP_ERR;
    }

    if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = bc->lex.previous_token_src;

    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

    i32 result = resolve_id(gab, bc, val_name, NULL);

    switch (result) {

    case COMP_ID_NOT_FOUND: {
      i32 loc = compile_local(gab, bc, val_name, flags);

      if (loc < 0)
        return COMP_ERR;

      locals[local_count] = loc;
      break;
    }

    case COMP_RESOLVED_TO_LOCAL:
    case COMP_RESOLVED_TO_UPVALUE: {
      compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }

    default:
      compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                     "While compiling let expression");
      return COMP_ERR;
    }

    local_count++;

  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

initializer:
  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (compile_tuple(gab, bc, local_count, NULL) < 0)
      return COMP_ERR;

    break;
  }

  case COMP_TOKEN_NO_MATCH:
    compiler_error(bc, GAB_MISSING_INITIALIZER, "");
    return COMP_ERR;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling 'let' expression");
    return COMP_ERR;
  }

  pop_slot(bc, local_count);

  while (local_count--) {
    u8 local = locals[local_count];
    initialize_local(bc, local);

    if (local_count > 0) {
      gab_module_push_store_local(mod(bc), local, bc->previous_token, bc->line,
                                  bc->lex.previous_token_src);
      push_pop(bc, 1);
    } else {
      gab_module_push_store_local(mod(bc), local, bc->previous_token, bc->line,
                                  bc->lex.previous_token_src);
    }
  }

  push_slot(bc, 1);
  return COMP_OK;
}

// Forward decl
i32 compile_definition(gab_engine *gab, gab_bc *bc, s_i8 name);

i32 compile_rec_internal_item(gab_engine *gab, gab_bc *bc) {
  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {

    gab_obj_string *obj =
        gab_obj_string_create(gab, bc->lex.previous_token_src);

    gab_value val_name = GAB_VAL_OBJ(obj);

    if (push_slot(bc, 1) < 0)
      return COMP_ERR;

    // Push the constant onto the stack.
    push_op(bc, OP_CONSTANT);
    push_short(bc, add_constant(mod(bc), val_name));

    // Compile the expression if theres an '=' , or look for a local with
    // the name and use that as the value.
    switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

    case COMP_OK: {
      if (compile_expression(gab, bc) < 0)
        return COMP_ERR;

      return COMP_OK;
    }

    case COMP_TOKEN_NO_MATCH: {
      u8 value_in;

      i32 result = resolve_id(gab, bc, val_name, &value_in);

      if (push_slot(bc, 1) < 0)
        return COMP_ERR;

      switch (result) {

      case COMP_RESOLVED_TO_LOCAL:
        push_load_local(bc, value_in);
        break;

      case COMP_RESOLVED_TO_UPVALUE:
        push_load_upvalue(bc, value_in);
        break;

      case COMP_ID_NOT_FOUND:
        push_op(bc, OP_PUSH_TRUE);
        break;

      default:
        compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                       "While compiling object literal");
        return COMP_ERR;
      }

      return COMP_OK;
    }

    default:
      goto err;
    }
  }

  if (match_and_eat_token(bc, TOKEN_LBRACE)) {

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (compile_expression(gab, bc) < 0)
        return COMP_ERR;
    } else {
      push_slot(bc, 1);

      push_op(bc, OP_PUSH_TRUE);
    }

    return COMP_OK;
  }

err:
  eat_token(bc);
  compiler_error(bc, GAB_UNEXPECTED_TOKEN, "While compiling object literal");
  return COMP_ERR;
}

i32 compile_rec_internals(gab_engine *gab, gab_bc *bc) {
  u8 size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (match_and_eat_token(bc, TOKEN_RBRACK) == COMP_TOKEN_NO_MATCH) {

    if (compile_rec_internal_item(gab, bc) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    size++;
  };

  return size;
}

i32 compile_record(gab_engine *gab, gab_bc *bc, gab_value name) {
  boolean is_def = !GAB_VAL_IS_NIL(name);
  i32 name_const;

  if (is_def) {
    name_const = add_constant(mod(bc), name);
  }

  i32 size = compile_rec_internals(gab, bc);

  if (size < 0)
    return COMP_ERR;

  pop_slot(bc, size * 2);

  push_op(bc, is_def ? OP_RECORD_DEF : OP_RECORD);

  if (is_def)
    push_short(bc, name_const);

  push_byte(bc, size);

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;
  return COMP_OK;
}

i32 compile_array(gab_engine *gab, gab_bc *bc) {
  boolean vse;
  i32 size = compile_lst_internals(gab, bc, &vse);

  if (size < 0)
    return COMP_ERR;

  gab_module_push_tuple(mod(bc), size, vse, bc->previous_token, bc->line,
                        bc->lex.previous_token_src);

  pop_slot(bc, size);

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;
  return COMP_OK;
}

i32 compile_definition(gab_engine *gab, gab_bc *bc, s_i8 name) {
  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

    u8 local = add_local(gab, bc, val_name, 0);
    initialize_local(bc, local);

    if (compile_record(gab, bc, val_name) < 0)
      return COMP_ERR;

    push_op(bc, OP_TYPE);

    gab_module_push_store_local(mod(bc), local, bc->previous_token, bc->line,
                                bc->lex.previous_token_src);

    return COMP_OK;
  }

  // Const variable
  if (match_token(bc, TOKEN_EQUAL) || match_token(bc, TOKEN_COMMA)) {
    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

    u8 local = add_local(gab, bc, val_name, 0);

    match_and_eat_token(bc, TOKEN_COMMA);

    return compile_var_decl(gab, bc, 0, local);
  }

  // From now on, we know its a function definition.
  // Function names can end in a ? or a !
  // or, if the name is op
  if (match_and_eat_token(bc, TOKEN_QUESTION))
    name.len++;
  else if (match_and_eat_token(bc, TOKEN_BANG))
    name.len++;

  gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

  // Now compile the impl
  if (compile_message(gab, bc, val_name) < 0)
    return COMP_ERR;

  // Create a local to store the new function in
  u8 local = add_local(gab, bc, val_name, 0);
  initialize_local(bc, local);

  push_store_local(bc, local);

  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

i32 compile_exp_blk(gab_engine *gab, gab_bc *bc, boolean assignable) {

  // We are an anonyumous function
  if (compile_block(gab, bc) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_then(gab_engine *gab, gab_bc *bc, boolean assignable) {
  u64 then_jump =
      gab_module_push_jump(mod(bc), OP_JUMP_IF_FALSE, bc->previous_token,
                           bc->line, bc->lex.previous_token_src);

  pop_slot(bc, 1);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod(bc), then_jump);

  return COMP_OK;
}

i32 compile_exp_else(gab_engine *gab, gab_bc *bc, boolean assignable) {
  u64 then_jump =
      gab_module_push_jump(mod(bc), OP_JUMP_IF_TRUE, bc->previous_token,
                           bc->line, bc->lex.previous_token_src);

  pop_slot(bc, 1);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod(bc), then_jump);

  return COMP_OK;
}

i32 compile_exp_mch(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  u64 next = 0;

  v_u64 done_jumps;
  v_u64_create(&done_jumps, 8);

  // While we don't match the closing question
  while (match_and_eat_token(bc, TOKEN_ELSE) == COMP_TOKEN_NO_MATCH) {
    if (next != 0)
      gab_module_patch_jump(mod(bc), next);

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_MATCH);

    next =
        gab_module_push_jump(mod(bc), OP_POP_JUMP_IF_FALSE, bc->previous_token,
                             bc->line, bc->lex.previous_token_src);

    pop_slot(bc, 1);

    if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
      return COMP_ERR;

    if (optional_newline(bc) < 0)
      return COMP_ERR;

    if (compile_expressions(gab, bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    pop_slot(bc, 1);

    // Push a jump out of the match statement at the end of every case.
    v_u64_push(&done_jumps,
               gab_module_push_jump(mod(bc), OP_JUMP, bc->previous_token,
                                    bc->line, bc->lex.previous_token_src));
  }

  // If none of the cases match, the last jump should end up here.
  gab_module_patch_jump(mod(bc), next);

  // Pop the pattern that we never matched
  push_pop(bc, 1);

  if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  for (i32 i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    gab_module_patch_jump(mod(bc), v_u64_val_at(&done_jumps, i));
  }

  v_u64_destroy(&done_jumps);

  return push_slot(bc, 1);
}

/*
 * Infix is expression.
 */
i32 compile_exp_is(gab_engine *gab, gab_bc *bc, boolean assignable) {
  push_op(bc, OP_TYPE);

  if (compile_exp_prec(gab, bc, PREC_EQUALITY) < 0)
    return COMP_ERR;

  // TODO: FIXME
  // push_op(bc, mod, OP_EQUAL);

  pop_slot(bc, 1);

  return COMP_OK;
}

i32 compile_exp_bin(gab_engine *gab, gab_bc *bc, boolean assignable) {
  gab_token op = bc->previous_token;
  u64 line = bc->line;
  s_i8 src = bc->lex.previous_token_src;

  u16 m;

  switch (op) {
  case TOKEN_MINUS:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_SUB));
    break;

  case TOKEN_PLUS:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_ADD));
    break;

  case TOKEN_STAR:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_MUL));
    break;

  case TOKEN_SLASH:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_DIV));
    break;

  case TOKEN_PERCENT:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_MOD));
    break;

  case TOKEN_PIPE:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_BOR));
    break;

  case TOKEN_AMPERSAND:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_BND));
    break;

  case TOKEN_EQUAL_EQUAL:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_EQ));
    break;

  case TOKEN_LESSER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_LT));
    break;

  case TOKEN_LESSER_EQUAL:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_LTE));
    break;

  case TOKEN_LESSER_LESSER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_LSH));
    break;

  case TOKEN_GREATER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_GT));
    break;

  case TOKEN_GREATER_EQUAL:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_GTE));
    break;

  case TOKEN_GREATER_GREATER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_RSH));
    break;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling binary expression");
    return COMP_ERR;
  }

  i32 result = compile_exp_prec(gab, bc, get_rule(op).prec + 1);

  pop_slot(bc, 1);

  if (result < 0)
    return COMP_ERR;

  gab_module_push_send(mod(bc), 1, m, false, op, line, src);
  return VAR_EXP;
}

i32 compile_exp_una(gab_engine *gab, gab_bc *bc, boolean assignable) {
  gab_token op = bc->previous_token;

  i32 result = compile_exp_prec(gab, bc, PREC_UNARY);

  if (result < 0)
    return COMP_ERR;

  switch (op) {
  case TOKEN_MINUS:
    push_op(bc, OP_NEGATE);
    return COMP_OK;

  case TOKEN_NOT:
    push_op(bc, OP_NOT);
    return COMP_OK;

  case TOKEN_QUESTION:
    push_op(bc, OP_TYPE);
    return COMP_OK;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling unary expression");
    return COMP_ERR;
  }
}

i32 encode_codepoint(i8 *out, u32 utf) {
  if (utf <= 0x7F) {
    // Plain ASCII
    out[0] = (char)utf;
    return 1;
  } else if (utf <= 0x07FF) {
    // 2-byte unicode
    out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
    return 2;
  } else if (utf <= 0xFFFF) {
    // 3-byte unicode
    out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
    return 3;
  } else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
    return 4;
  } else {
    // error - use replacement character
    out[0] = (char)0xEF;
    out[1] = (char)0xBF;
    out[2] = (char)0xBD;
    return 3;
  }
}

/*
 * Returns null if an error occured.
 */
a_i8 *parse_raw_str(gab_bc *bc, s_i8 raw_str) {
  // The parsed string will be at most as long as the raw string.
  // (\n -> one char)
  i8 buffer[raw_str.len];
  u64 buf_end = 0;

  // Skip the first and last character (")
  for (u64 i = 1; i < raw_str.len - 1; i++) {
    i8 c = raw_str.data[i];

    if (c == '\\') {

      switch (raw_str.data[i + 1]) {

      case 'n':
        buffer[buf_end++] = '\n';
        break;
      case 't':
        buffer[buf_end++] = '\t';
        break;
      case '{':
        buffer[buf_end++] = '{';
        break;
      case 'u':
        i += 2;

        if (raw_str.data[i] != '[') {
          return NULL;
        }

        i++;

        u8 cpl = 0;
        char codepoint[8] = {0};

        while (raw_str.data[i] != ']') {

          if (cpl == 7)
            return NULL;

          codepoint[cpl++] = raw_str.data[i++];
        }

        i++;

        u32 cp = strtol(codepoint, NULL, 16);

        i32 result = encode_codepoint(buffer + buf_end, cp);

        buf_end += result;

        break;
      case '\\':
        buffer[buf_end++] = '\\';
        break;
      default:
        buffer[buf_end++] = c;
        continue;
      }

      i++;

    } else {
      buffer[buf_end++] = c;
    }
  }

  return a_i8_create(buffer, buf_end);
};

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_str(gab_engine *gab, gab_bc *bc, boolean assignable) {
  s_i8 raw_token = bc->lex.previous_token_src;

  a_i8 *parsed = parse_raw_str(bc, raw_token);

  if (parsed == NULL) {
    compiler_error(bc, GAB_MALFORMED_STRING, "");
    return COMP_ERR;
  }

  gab_obj_string *obj =
      gab_obj_string_create(gab, s_i8_create(parsed->data, parsed->len));

  a_i8_destroy(parsed);

  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(obj)));

  return push_slot(bc, 1);
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_itp(gab_engine *gab, gab_bc *bc, boolean assignable) {
  i32 result = COMP_OK;
  u8 n = 0;
  do {
    if (compile_exp_str(gab, bc, assignable) < 0)
      return COMP_ERR;

    n++;

    if (match_token(bc, TOKEN_INTERPOLATION_END)) {
      goto fin;
    }

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;
    n++;

  } while ((result = match_and_eat_token(bc, TOKEN_INTERPOLATION)));

fin:
  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_INTERPOLATION_END) < 0)
    return COMP_ERR;

  if (compile_exp_str(gab, bc, assignable) < 0)
    return COMP_ERR;
  n++;

  // Concat the final string.
  push_op(bc, OP_INTERPOLATE);
  push_byte(bc, n);

  pop_slot(bc, n - 1);

  return COMP_OK;
}

i32 compile_exp_grp(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_num(gab_engine *gab, gab_bc *bc, boolean assignable) {
  f64 num = strtod((char *)bc->lex.previous_token_src.data, NULL);
  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), GAB_VAL_NUMBER(num)));

  return push_slot(bc, 1);
}

i32 compile_exp_bool(gab_engine *gab, gab_bc *bc, boolean assignable) {
  push_op(bc, bc->previous_token == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);

  return push_slot(bc, 1);
}

i32 compile_exp_nil(gab_engine *gab, gab_bc *bc, boolean assignable) {
  push_op(bc, OP_PUSH_NIL);

  return push_slot(bc, 1);
}

i32 compile_exp_def(gab_engine *gab, gab_bc *bc, boolean assignable) {
  eat_token(bc);
  s_i8 name = {0};
  switch (bc->previous_token) {
  case TOKEN_IDENTIFIER:
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_LESSER:
  case TOKEN_LESSER_EQUAL:
  case TOKEN_LESSER_LESSER:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_GREATER_GREATER:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_PIPE:
  case TOKEN_AMPERSAND:
    name = bc->lex.previous_token_src;
    break;
  case TOKEN_LPAREN:
    name = bc->lex.previous_token_src;
    if (match_and_eat_token(bc, TOKEN_RPAREN)) {
      name.len++;
      break;
    }
    break;
  case TOKEN_LBRACE: {
    name = bc->lex.previous_token_src;
    if (match_and_eat_token(bc, TOKEN_RBRACE)) {
      name.len++;
      break;
    }

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (!expect_token(bc, TOKEN_RBRACE))
        return COMP_ERR;

      name.len += 2;
      break;
    }
  }
  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "While compiling definition");
    return COMP_ERR;
  }

  if (compile_definition(gab, bc, name) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_arr(gab_engine *gab, gab_bc *bc, boolean assignable) {
  return compile_array(gab, bc);
}

i32 compile_exp_rec(gab_engine *gab, gab_bc *bc, boolean assignable) {
  return compile_record(gab, bc, GAB_VAL_NIL());
}

i32 compile_exp_imp(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (!compile_expression(gab, bc))
    return COMP_ERR;

  i32 local = add_local(gab, bc, GAB_STRING(""), 0);

  if (local < 0)
    return COMP_ERR;

  initialize_local(bc, local);

  push_store_local(bc, local);

  push_pop(bc, 1);

  pop_slot(bc, 1);

  if (push_impl(gab, bc, local) < 0)
    return COMP_ERR;

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  if (!expect_token(bc, TOKEN_END))
    return COMP_ERR;

  push_load_local(bc, local);

  pop_impl(bc);

  return COMP_OK;
}

i32 compile_exp_let(gab_engine *gab, gab_bc *bc, boolean assignable) {
  return compile_var_decl(gab, bc, GAB_VARIABLE_FLAG_MUTABLE, 0);
}

i32 compile_exp_idn(gab_engine *gab, gab_bc *bc, boolean assignable) {
  s_i8 name = bc->lex.previous_token_src;

  gab_value id_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

  u8 var;
  boolean is_local_var = true;

  switch (resolve_id(gab, bc, id_name, &var)) {
  case COMP_RESOLVED_TO_LOCAL: {
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    is_local_var = false;
    break;
  }

  case COMP_ID_NOT_FOUND: {
    compiler_error(bc, GAB_MISSING_IDENTIFIER, "Could not find '%.*s", name.len,
                   name.data);
    return COMP_ERR;
  }

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "While compiling identifier");
    return COMP_ERR;
  }

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
      return COMP_ERR;
    }

    if (!is_local_var) {
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                     "Cannot assign to captured values");
      return COMP_ERR;
    }

    gab_bc_frame *frame = peek_frame(bc, 0);
    if (!(frame->locals_flag[var] & GAB_VARIABLE_FLAG_MUTABLE)) {
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                     "Variable is not mutable");
      return COMP_ERR;
    }

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    push_store_local(bc, var);

    break;
  }

  case COMP_TOKEN_NO_MATCH:
    if (is_local_var)
      push_load_local(bc, var);
    else
      push_load_upvalue(bc, var);
    break;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling 'identifier' expression");
    return COMP_ERR;
  }

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;
  return COMP_OK;
}

i32 compile_exp_idx(gab_engine *gab, gab_bc *bc, boolean assignable) {
  gab_token prop_tok = bc->previous_token;
  u64 prop_line = bc->line;
  s_i8 prop_src = bc->lex.previous_token_src;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (assignable) {
      if (compile_expression(gab, bc) < 0)
        return COMP_ERR;

      pop_slot(bc, 1);

      u16 m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_SET));

      gab_module_push_send(mod(bc), 2, m, false, prop_tok, prop_line, prop_src);
    } else {
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                     "While compiling 'index' expression");
      return COMP_ERR;
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    u16 m = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_GET));

    gab_module_push_send(mod(bc), 1, m, false, prop_tok, prop_line, prop_src);
    break;
  }

  default:
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                   "While compiling 'index' expression");
    return COMP_ERR;
  }

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;
  return VAR_EXP;
}

i32 compile_exp_dot(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (compile_property(gab, bc, assignable) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_arg_list(gab_engine *gab, gab_bc *bc, boolean *vse_out) {
  i32 nargs = 0;

  if (!match_token(bc, TOKEN_RPAREN)) {

    nargs = compile_tuple(gab, bc, VAR_EXP, vse_out);

    if (nargs < 0)
      return COMP_ERR;
  }

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return nargs;
}

#define FLAG_HAS_PAREN 1
#define FLAG_HAS_BRACK 2

i32 compile_arguments(gab_engine *gab, gab_bc *bc, boolean *vse_out, u8 flags) {
  i32 result = 0;

  if (flags & FLAG_HAS_PAREN || match_and_eat_token(bc, TOKEN_LPAREN)) {
    // Normal function args
    result = compile_arg_list(gab, bc, vse_out);
    if (result < 0)
      return COMP_ERR;
  }

  if (flags & FLAG_HAS_BRACK || match_and_eat_token(bc, TOKEN_LBRACK)) {
    // record argument
    if (compile_record(gab, bc, GAB_VAL_NIL()) < 0)
      return COMP_ERR;

    result += 1 + *vse_out;

    // result += 1 + *vse_out;
    *vse_out = false;
  }

  if (match_and_eat_token(bc, TOKEN_DO)) {

    // We are an anonyumous function
    if (compile_block(gab, bc) < 0)
      return COMP_ERR;

    result += 1 + *vse_out;
    *vse_out = false;
  }

  return result;
}

i32 compile_exp_emp(gab_engine *gab, gab_bc *bc, boolean assignable) {
  s_i8 message = trim_prev_tok(bc);
  gab_token tok = bc->previous_token;
  u64 line = bc->line;

  gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, message));

  u16 m = add_message_constant(gab, mod(bc), val_name);

  push_op(bc, OP_PUSH_NIL);

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;

  boolean vse;
  i32 args = compile_arguments(gab, bc, &vse, 0);

  gab_module_push_send(mod(bc), args, m, vse, tok, line, message);

  pop_slot(bc, args + 1);

  return VAR_EXP;
}

i32 compile_exp_amp(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (!expect_token(bc, TOKEN_MESSAGE))
    return COMP_ERR;

  s_i8 message = trim_prev_tok(bc);

  gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, message));

  u16 f = add_message_constant(gab, mod(bc), val_name);

  push_op(bc, OP_CONSTANT);
  push_short(bc, f);

  return push_slot(bc, 1);
}

i32 compile_exp_snd(gab_engine *gab, gab_bc *bc, boolean assignable) {
  s_i8 prev_src = bc->lex.previous_token_src;
  gab_token prev_tok = bc->previous_token;
  u64 prev_line = bc->line;
  s_i8 message = trim_prev_tok(bc);

  gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, message));

  u16 m = add_message_constant(gab, mod(bc), val_name);

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  gab_module_push_send(mod(bc), result, m, vse, prev_tok, prev_line, prev_src);

  if (push_slot(bc, 1) < 0)
    return COMP_ERR;
  return VAR_EXP;
}

i32 compile_exp_cal(gab_engine *gab, gab_bc *bc, boolean assignable) {
  gab_token call_tok = bc->previous_token;
  u64 call_line = bc->line;
  s_i8 call_src = bc->lex.previous_token_src;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, FLAG_HAS_PAREN);

  if (result < 0)
    return COMP_ERR;

  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result);

  u16 msg = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_CAL));

  gab_module_push_send(mod(bc), result, msg, vse, call_tok, call_line,
                       call_src);

  return VAR_EXP;
}

i32 compile_exp_rcal(gab_engine *gab, gab_bc *bc, boolean assignable) {
  gab_token call_tok = bc->previous_token;
  u64 call_line = bc->line;
  s_i8 call_src = bc->lex.previous_token_src;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, FLAG_HAS_BRACK);

  pop_slot(bc, 1);

  u16 msg = add_message_constant(gab, mod(bc), GAB_STRING(GAB_MESSAGE_CAL));

  gab_module_push_send(mod(bc), result, msg, vse, call_tok, call_line,
                       call_src);

  return VAR_EXP;
}

i32 compile_exp_and(gab_engine *gab, gab_bc *bc, boolean assignable) {
  u64 end_jump =
      gab_module_push_jump(mod(bc), OP_LOGICAL_AND, bc->previous_token,
                           bc->line, bc->lex.previous_token_src);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, PREC_AND) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

i32 compile_exp_or(gab_engine *gab, gab_bc *bc, boolean assignable) {
  u64 end_jump =
      gab_module_push_jump(mod(bc), OP_LOGICAL_OR, bc->previous_token, bc->line,
                           bc->lex.previous_token_src);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, PREC_OR) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

i32 compile_exp_prec(gab_engine *gab, gab_bc *bc, gab_precedence prec) {
  if (eat_token(bc) < 0)
    return COMP_ERR;

  gab_compile_rule rule = get_rule(bc->previous_token);

  if (rule.prefix == NULL) {
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression");
    return COMP_ERR;
  }

  boolean assignable = prec <= PREC_ASSIGNMENT;

  i32 have = rule.prefix(gab, bc, assignable);
  if (have < 0)
    return COMP_ERR;

  while (prec <= get_rule(bc->current_token).prec) {
    if (have < 0)
      return COMP_ERR;

    if (eat_token(bc) < 0)
      return COMP_ERR;

    rule = get_rule(bc->previous_token);

    if (rule.infix != NULL) {
      // Treat this as an infix expression.
      have = rule.infix(gab, bc, assignable);
    }
  }

  // TODO: See if this actually does anything.
  if (assignable && match_token(bc, TOKEN_EQUAL)) {
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  return have;
}

/*
 * For loops are broken
 */
i32 compile_exp_for(gab_engine *gab, gab_bc *bc, boolean assignable) {
  down_scope(bc);

  u8 local_start = peek_frame(bc, 0)->next_local;
  u16 loop_locals = 0;
  i32 result;

  do {
    if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = bc->lex.previous_token_src;

    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

    i32 loc = compile_local(gab, bc, val_name, 0);

    if (loc < 0)
      return COMP_ERR;

    initialize_local(bc, loc);
    loop_locals++;
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  u8 iter = add_local(gab, bc, GAB_VAL_NIL(), 0);
  initialize_local(bc, iter);

  if (result == COMP_ERR)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_IN) < 0)
    return COMP_ERR;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (push_slot(bc, loop_locals) < 0)
    return COMP_ERR;

  gab_module_try_patch_vse(mod(bc), VAR_EXP);

  u64 loop = gab_module_push_loop(mod(bc));

  u64 jump_start = gab_module_push_iter(mod(bc), loop_locals, local_start,
                                        bc->previous_token, bc->line,
                                        bc->lex.previous_token_src);
  pop_slot(bc, loop_locals);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  // Load and call the effect member
  gab_module_push_next(mod(bc), iter, bc->previous_token, bc->line,
                       bc->lex.previous_token_src);

  pop_slot(bc, 1);

  gab_module_patch_loop(mod(bc), loop, bc->previous_token, bc->line,
                        bc->lex.previous_token_src);

  gab_module_patch_jump(mod(bc), jump_start);

  up_scope(bc);

  return COMP_OK;
}

i32 compile_exp_lop(gab_engine *gab, gab_bc *bc, boolean assignable) {
  down_scope(bc);

  u64 loop = gab_module_push_loop(mod(bc));

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (match_and_eat_token(bc, TOKEN_UNTIL)) {
    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    u64 jump =
        gab_module_push_jump(mod(bc), OP_POP_JUMP_IF_TRUE, bc->previous_token,
                             bc->line, bc->lex.previous_token_src);

    gab_module_patch_loop(mod(bc), loop, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);

    gab_module_patch_jump(mod(bc), jump);
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    gab_module_patch_loop(mod(bc), loop, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);
  }

  push_op(bc, OP_PUSH_NIL);

  up_scope(bc);
  return COMP_OK;
}

i32 compile_exp_sym(gab_engine *gab, gab_bc *bc, boolean assignable) {
  s_i8 name = trim_prev_tok(bc);

  gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

  gab_obj_symbol *sym = gab_obj_symbol_create(gab, val_name);

  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(sym)));

  return COMP_OK;
}

i32 compile_exp_yld(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (push_slot(bc, 1) < 0)
    return COMP_ERR;

  if (match_token(bc, TOKEN_NEWLINE)) {
    gab_module_push_yield(mod(bc), 0, false, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);
    return VAR_EXP;
  }

  boolean vse;
  i32 result = compile_tuple(gab, bc, VAR_EXP, &vse);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_module_push_yield(mod(bc), result, vse, bc->previous_token, bc->line,
                        bc->lex.previous_token_src);

  pop_slot(bc, result);

  return VAR_EXP;
}

i32 compile_exp_rtn(gab_engine *gab, gab_bc *bc, boolean assignable) {
  if (match_token(bc, TOKEN_NEWLINE)) {
    push_op(bc, OP_PUSH_NIL);
    gab_module_push_return(mod(bc), 1, false, bc->previous_token, bc->line,
                           bc->lex.previous_token_src);

    if (push_slot(bc, 1) < 0)
      return COMP_ERR;
    pop_slot(bc, 1);
    return COMP_OK;
  }

  boolean vse;
  i32 result = compile_tuple(gab, bc, VAR_EXP, &vse);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_module_push_return(mod(bc), result, vse, bc->previous_token, bc->line,
                         bc->lex.previous_token_src);

  pop_slot(bc, result);
  return COMP_OK;
}

// All of the expression compiling functions follow the naming convention
// compile_exp_XXX.
#define NONE()                                                                 \
  { NULL, NULL, PREC_NONE, false }
#define PREFIX(fnc)                                                            \
  { compile_exp_##fnc, NULL, PREC_NONE, false }
#define INFIX(fnc, prec, is_multi)                                             \
  { NULL, compile_exp_##fnc, PREC_##prec, is_multi }
#define PREFIX_INFIX(pre, in, prec, is_multi)                                  \
  { compile_exp_##pre, compile_exp_##in, PREC_##prec, is_multi }

// ----------------Pratt Parsing Table ----------------------
const gab_compile_rule gab_bc_rules[] = {
    PREFIX(let),                       // LET
    INFIX(then, AND, false),                        // THEN
    INFIX(else, OR, false),                            // ELSE
    PREFIX(blk),                       // DO
    PREFIX(for),                       // FOR
    INFIX(mch, MATCH, false),                       //MATCH 
    NONE(),                            // IN
    INFIX(is, COMPARISON, false),                            // IS
    NONE(),                            // END
    PREFIX(def),                       // DEF
    PREFIX(rtn),                       // RETURN
    PREFIX(yld),// YIELD
    PREFIX(lop),                       // LOOP
    NONE(),                       // UNTIL
    PREFIX(imp),                       // IMPL
    INFIX(bin, TERM, false),                  // PLUS
    PREFIX_INFIX(una, bin, TERM, false),      // MINUS
    INFIX(bin, FACTOR, false),                // STAR
    INFIX(bin, FACTOR, false),                // SLASH
    INFIX(bin, FACTOR, false),                // PERCENT
    NONE(),                            // COMMA
    PREFIX(emp),       // COLON
    PREFIX_INFIX(amp, bin, BITWISE_AND, false),           // AMPERSAND
    NONE(),           // DOLLAR
    PREFIX(sym), // SYMBOL
    INFIX(dot, PROPERTY, true), // PROPERTY
    PREFIX_INFIX(emp, snd, SEND, true), // MESSAGE
    INFIX(dot, PROPERTY, true),              // DOT
    NONE(),                  // DOT_DOT
    NONE(),                            // EQUAL
    INFIX(bin, EQUALITY, false),              // EQUALEQUAL
    PREFIX(una),                            // QUESTION
    NONE(),                      // BANG
    NONE(),                            // AT
    NONE(),                            // COLON_EQUAL
    INFIX(bin, COMPARISON, false),            // LESSER
    INFIX(bin, EQUALITY, false),              // LESSEREQUAL
    INFIX(bin, TERM, false),              // LESSEREQUAL
    INFIX(bin, COMPARISON, false),            // GREATER
    INFIX(bin, EQUALITY, false),              // GREATEREQUAL
    INFIX(bin, TERM, false),                            // GREATER_GREATER
    NONE(),                            // ARROW
    NONE(),                            // FATARROW
    INFIX(and, AND, false),                   // AND
    INFIX(or, OR, false),                     // OR
    PREFIX(una),                       // NOT
    PREFIX_INFIX(arr, idx, PROPERTY, false),  // LBRACE
    NONE(),                            // RBRACE
    PREFIX_INFIX(rec, rcal, SEND, false), // LBRACK
    NONE(),                            // RBRACK
    PREFIX_INFIX(grp, cal, SEND, false),     // LPAREN
    NONE(),                            // RPAREN
    INFIX(bin, BITWISE_OR, false),            // PIPE
    NONE(),                            // SEMI
    PREFIX(idn),                       // ID
    PREFIX(str),                       // STRING
    PREFIX(itp),                       // INTERPOLATION
    NONE(),                       // INTERPOLATION END
    PREFIX(num),                       // NUMBER
    PREFIX(bool),                      // FALSE
    PREFIX(bool),                      // TRUE
    PREFIX(nil),                      // TRUE
    NONE(),                      // NEWLINE
    NONE(),                            // EOF
    NONE(),                            // ERROR
};

gab_compile_rule get_rule(gab_token k) { return gab_bc_rules[k]; }

gab_module *compile(gab_engine *gab, gab_bc *bc, gab_value name, u8 narguments,
                    gab_value arguments[narguments]) {
  gab_module *new_mod = down_frame(gab, bc, name, false);

  if (eat_token(bc) == COMP_ERR)
    return NULL;

  for (u8 i = 0; i < narguments; i++) {
    initialize_local(bc, add_local(gab, bc, arguments[i], 0));
  }

  if (compile_expressions_body(gab, bc) < 0)
    return NULL;

  gab_module_push_return(mod(bc), 1, false, bc->previous_token, bc->line,
                         bc->lex.previous_token_src);

  gab_obj_prototype *p = up_frame(gab, bc, narguments, false);

  add_constant(new_mod, GAB_VAL_OBJ(p));

  return new_mod;
}

gab_module *gab_bc_compile_send(gab_engine *gab, gab_value name,
                                gab_value receiver, u8 argc,
                                gab_value argv[argc]) {

  gab_module *mod = gab_module_create(name, NULL, gab->modules);
  gab->modules = mod;

  u16 message = add_message_constant(gab, mod, name);

  u16 constant = add_constant(mod, receiver);

  gab_module_push_op(mod, OP_CONSTANT, 0, 0, (s_i8){0});
  gab_module_push_short(mod, constant, 0, 0, (s_i8){0});

  for (u8 i = 0; i < argc; i++) {
    u16 constant = add_constant(mod, receiver);

    gab_module_push_op(mod, OP_CONSTANT, 0, 0, (s_i8){0});
    gab_module_push_short(mod, constant, 0, 0, (s_i8){0});
  }

  gab_module_push_send(mod, argc, message, false, 0, 0, (s_i8){0});

  gab_module_push_return(mod, 1, false, 0, 0, (s_i8){0});

  gab_obj_prototype *p =
      gab_obj_prototype_create(gab, mod, argc, argc, 0, 0, false, NULL, NULL);

  add_constant(mod, GAB_VAL_OBJ(p));

  gab_obj_block *main = gab_obj_block_create(gab, p);

  mod->main = add_constant(mod, GAB_VAL_OBJ(main));

  return mod;
}

gab_module *gab_bc_compile(gab_engine *gab, gab_value name, s_i8 source,
                           u8 flags, u8 narguments,
                           gab_value arguments[narguments]) {
  gab_bc bc;
  gab_bc_create(&bc, gab_source_create(gab, source), flags);

  gab_module *mod = compile(gab, &bc, name, narguments, arguments);

  if (!mod) {
    return NULL;
  }

  if (flags & GAB_FLAG_DUMP_BYTECODE) {
    gab_module_dump(mod, s_i8_cstr("dump"));
  }

  return mod;
}

static void compiler_error(gab_bc *bc, gab_status e, const char *help_fmt,
                           ...) {
  if (bc->panic) {
    return;
  }

  bc->panic = true;

  if (bc->flags & GAB_FLAG_DUMP_ERROR) {
    gab_source *src = mod(bc)->source;

    va_list args;
    va_start(args, help_fmt);

    gab_lexer_finish_line(&bc->lex);

    gab_bc_frame *frame = &bc->frames[bc->nframe - 1];
    s_i8 curr_token = bc->lex.previous_token_src;
    u64 curr_src_index = bc->line - 1;
    s_i8 curr_src;

    if (curr_src_index < src->source->len) {
      curr_src = v_s_i8_val_at(&src->source_lines, curr_src_index);
    } else {
      curr_src = s_i8_cstr("");
    }

    i8 *curr_src_start = curr_src.data;
    i32 curr_src_len = curr_src.len;

    while (is_whitespace(*curr_src_start)) {
      curr_src_start++;
      curr_src_len--;
      if (curr_src_len == 0)
        break;
    }

    gab_value func_name =
        v_gab_constant_val_at(&frame->mod->constants, frame->mod->name);

    a_i8 *curr_under = a_i8_empty(curr_src_len);

    i8 *tok_start, *tok_end;

    tok_start = curr_token.data;
    tok_end = curr_token.data + curr_token.len;

    const char *tok = gab_token_names[bc->previous_token];

    for (u8 i = 0; i < curr_under->len; i++) {
      if (curr_src_start + i >= tok_start && curr_src_start + i < tok_end)
        curr_under->data[i] = '^';
      else
        curr_under->data[i] = ' ';
    }

    const char *curr_color = ANSI_COLOR_RED;

    const char *curr_box = "\u256d";

    fprintf(stderr,
            "[" ANSI_COLOR_GREEN "%V" ANSI_COLOR_RESET
            "] Error near " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET
            ":\n\t%s%s %.4lu " ANSI_COLOR_RESET "%.*s"
            "\n\t\u2502      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET
            "\n\t\u2570\u2500> ",
            func_name, tok, curr_box, curr_color, curr_src_index + 1,
            curr_src_len, curr_src_start, (i32)curr_under->len,
            curr_under->data);

    a_i8_destroy(curr_under);

    fprintf(stderr, ANSI_COLOR_YELLOW "%s. " ANSI_COLOR_RESET ANSI_COLOR_GREEN,
            gab_status_names[e]);

    vfprintf(stderr, help_fmt, args);

    fprintf(stderr, "\n" ANSI_COLOR_RESET);

    va_end(args);
  }
}
