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

typedef struct {
  gab_module *mod;

  i32 scope_depth;

  u8 narguments;

  u16 next_slot;
  u16 nslots;

  u8 next_local;
  u8 nlocals;

  u8 nupvalues;

  u8 local_flags[GAB_LOCAL_MAX];
  i32 local_depths[GAB_LOCAL_MAX];
  u16 local_names[GAB_LOCAL_MAX];

  u8 upv_flags[GAB_UPVALUE_MAX];
  u8 upv_indexes[GAB_UPVALUE_MAX];
} frame;

typedef enum {
  kNEW_LOCAL,
  kEXISTING_LOCAL,
  kPROP,
  kINDEX,
} lvalue_k;

typedef struct {
  lvalue_k kind;

  u16 slot;

  union {
    gab_value local;

    u16 property;
  } as;

} lvalue;

#define T lvalue
#include "include/vector.h"

typedef enum {
  kIMPL,
  kFRAME,
  kTUPLE,
  kASSIGNMENT_TARGET,
  kLOOP,
} context_k;

typedef struct {
  context_k kind;

  union {
    struct {
      u8 ctx, local;
    } impl;

    v_lvalue assignment_target;

    frame frame;
  } as;
} context;

typedef struct {
  gab_lexer lex;

  u64 line;

  u8 flags;

  boolean panic;

  u8 ncontext;
  context contexts[cGAB_FUNCTION_DEF_NESTING_MAX];
} bc;

const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
};

/*
  Precedence rules for the parsing of expressions.
*/
typedef enum prec_k {
  kNONE,
  kASSIGNMENT,  // =
  kOR,          // or, infix else
  kAND,         // and, infix then
  kMATCH,       // match
  kEQUALITY,    // ==
  kCOMPARISON,  // < > <= >=
  kBITWISE_OR,  // |
  kBITWISE_AND, // &
  kTERM,        // + -
  kFACTOR,      // * /
  kUNARY,       // ! - not
  kSEND,        // ( { :
  kPROPERTY,    // .
  kPRIMARY
} prec_k;

/*
  Compile rules used for Pratt parsing of expressions.
*/
typedef i32 (*gab_compile_func)(gab_engine *, bc *, boolean);

typedef struct gab_compile_rule gab_compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  prec_k prec;
  boolean multi_line;
};

void gab_bc_create(bc *self, gab_source *source, u8 flags) {
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
  COMP_CONTEXT_NOT_FOUND = -9,
  COMP_MAX = INT32_MAX,
};

static void compiler_error(bc *bc, gab_status e, const char *help_fmt, ...);

static boolean match_token(bc *bc, gab_token tok);

static i32 eat_token(bc *bc);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline boolean match_token(bc *bc, gab_token tok) {
  return bc->lex.current_token == tok;
}

static inline boolean match_terminator(bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL) || match_token(bc, TOKEN_EOF);
}

// Returns less than 0 if there was an error, greater than 0 otherwise.
static i32 eat_token(bc *bc) {
  bc->lex.previous_token = bc->lex.current_token;
  bc->lex.current_token = gab_lexer_next(&bc->lex);
  bc->line = bc->lex.current_row;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    compiler_error(bc, bc->lex.status, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 match_and_eat_token(bc *bc, gab_token tok) {
  if (!match_token(bc, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

static inline i32 expect_token(bc *bc, gab_token tok) {
  if (!match_token(bc, tok)) {
    eat_token(bc);
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "Expected " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET,
                   gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(bc);
}

static inline i32 match_tokoneof(bc *bc, gab_token toka, gab_token tokb) {
  return match_token(bc, toka) || match_token(bc, tokb);
}

static inline i32 expect_oneof(bc *bc, gab_token toka, gab_token tokb) {
  if (match_and_eat_token(bc, toka))
    return COMP_OK;

  return expect_token(bc, tokb);
}

gab_value prev_id(gab_engine *gab, bc *bc) {
  s_i8 message = bc->lex.previous_token_src;
  return GAB_VAL_OBJ(gab_obj_string_create(gab, message));
}

s_i8 trim_prev_tok(bc *bc) {
  s_i8 message = bc->lex.previous_token_src;
  // SKip the ':' at the beginning
  message.data++;
  message.len--;

  return message;
}

static inline i32 peek_ctx(bc *bc, context_k kind, u8 depth) {
  i32 idx = bc->ncontext - 1;

  while (idx >= 0) {
    if (bc->contexts[idx].kind == kind)
      if (depth-- == 0)
        return idx;

    idx--;
  }

  return COMP_CONTEXT_NOT_FOUND;
}

static inline gab_module *mod(bc *bc) {
  i32 frame = peek_ctx(bc, kFRAME, 0);
  return bc->contexts[frame].as.frame.mod;
}

static inline u16 add_constant(gab_module *mod, gab_value value) {
  return gab_module_add_constant(mod, value);
}

static inline void push_op(bc *bc, gab_opcode op) {
  gab_module_push_op(mod(bc), op, bc->lex.previous_token, bc->line,
                     bc->lex.previous_token_src);
}

static inline void push_shift(bc *bc, u8 n) {
  if (n <= 1)
    return;

  gab_module_push_op(mod(bc), OP_SHIFT, bc->lex.previous_token, bc->line,
                     bc->lex.previous_token_src);

  gab_module_push_byte(mod(bc), n, bc->lex.previous_token, bc->line,
                       bc->lex.previous_token_src);
}

static inline void push_pop(bc *bc, u8 n) {
  gab_module_push_pop(mod(bc), n, bc->lex.previous_token, bc->line,
                      bc->lex.previous_token_src);
}

static inline void push_store_local(bc *bc, u8 local) {
  gab_module_push_store_local(mod(bc), local, bc->lex.previous_token, bc->line,
                              bc->lex.previous_token_src);
}

static inline void push_load_local(bc *bc, u8 local) {
  gab_module_push_load_local(mod(bc), local, bc->lex.previous_token, bc->line,
                             bc->lex.previous_token_src);
}

static inline void push_load_upvalue(bc *bc, u8 upv) {
  gab_module_push_load_upvalue(mod(bc), upv, bc->lex.previous_token, bc->line,
                               bc->lex.previous_token_src);
}

static inline void push_byte(bc *bc, u8 data) {
  gab_module_push_byte(mod(bc), data, bc->lex.previous_token, bc->line,
                       bc->lex.previous_token_src);
}

static inline void push_short(bc *bc, u16 data) {
  gab_module_push_short(mod(bc), data, bc->lex.previous_token, bc->line,
                        bc->lex.previous_token_src);
}

static inline u16 peek_slot(bc *bc) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  return f->next_slot;
}

static inline void push_slot(bc *bc, u16 n) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot + n >= UINT16_MAX) {
    compiler_error(bc, GAB_PANIC,
                   "Too many slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot += n;

  if (f->next_slot > f->nslots)
    f->nslots = f->next_slot;
}

static inline void pop_slot(bc *bc, u16 n) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot - n < 0) {
    compiler_error(bc, GAB_PANIC,
                   "Too few slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot -= n;
}

static void initialize_local(bc *bc, u8 local) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->local_depths[local] != -1)
    return;

  f->local_depths[local] = f->scope_depth;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static i32 add_local(gab_engine *gab, bc *bc, gab_value name, u8 flags) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->nlocals == GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  u8 local = f->next_local;
  f->local_names[local] = add_constant(mod(bc), name);
  f->local_depths[local] = -1;
  f->local_flags[local] = flags;

  if (++f->next_local > f->nlocals)
    f->nlocals = f->next_local;

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * upvalue.
 */
static i32 add_upvalue(bc *bc, u32 depth, u8 index, u8 flags) {
  i32 ctx = peek_ctx(bc, kFRAME, depth);

  frame *f = &bc->contexts[ctx].as.frame;
  u16 count = f->nupvalues;

  // Don't pull redundant upvalues
  for (int i = 0; i < count; i++) {
    if (f->upv_indexes[i] == index && (f->upv_flags[i]) == flags) {
      return i;
    }
  }

  if (count == GAB_UPVALUE_MAX) {
    compiler_error(bc, GAB_TOO_MANY_UPVALUES, "");
    return COMP_ERR;
  }

  f->upv_indexes[count] = index;
  f->upv_flags[count] = flags;
  f->nupvalues++;

  return count;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_LOCAL_NOT_FOUND if no matching local is found,
 * and otherwise the offset of the local.
 */
static i32 resolve_local(gab_engine *gab, bc *bc, gab_value name, u8 depth) {
  i32 ctx = peek_ctx(bc, kFRAME, depth);

  if (ctx < 0) {
    return COMP_LOCAL_NOT_FOUND;
  }

  frame *f = &bc->contexts[ctx].as.frame;

  for (i32 local = f->next_local - 1; local >= 0; local--) {
    gab_value other_local_name =
        v_gab_constant_val_at(&f->mod->constants, f->local_names[local]);

    if (name == other_local_name) {
      if (f->local_depths[local] == -1) {
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
static i32 resolve_upvalue(gab_engine *gab, bc *bc, gab_value name, u8 depth) {
  if (depth >= bc->ncontext) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  i32 local = resolve_local(gab, bc, name, depth + 1);

  if (local >= 0) {
    i32 ctx = peek_ctx(bc, kFRAME, depth + 1);
    frame *f = &bc->contexts[ctx].as.frame;

    u8 flags = f->local_flags[local] |= fCAPTURED;

    if (flags & fMUTABLE) {
      compiler_error(bc, GAB_CAPTURED_MUTABLE, "");
      return COMP_ERR;
    }

    return add_upvalue(bc, depth, local, flags | fLOCAL);
  }

  i32 upvalue = resolve_upvalue(gab, bc, name, depth + 1);
  if (upvalue >= 0) {
    i32 ctx = peek_ctx(bc, kFRAME, depth + 1);
    frame *f = &bc->contexts[ctx].as.frame;

    u8 flags = f->upv_flags[upvalue];
    return add_upvalue(bc, depth, upvalue, flags & ~fLOCAL);
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_RESOLVED_TO_LOCAL if the id was a local, and
 * COMP_RESOLVED_TO_UPVALUE if the id was an upvalue.
 */
static i32 resolve_id(gab_engine *gab, bc *bc, gab_value name, u8 *value_in) {
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

static inline i32 peek_scope(bc *bc) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;
  return f->scope_depth;
}

static void push_scope(bc *bc) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;
  f->scope_depth++;
}

static void pop_scope(bc *bc) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->scope_depth--;

  while (f->next_local > 1) {
    u8 local = f->next_local - 1;

    if (f->local_depths[local] <= f->scope_depth)
      break;

    f->next_local--;
  }
}

static inline boolean match_ctx(bc *bc, context_k kind) {
  return bc->contexts[bc->ncontext - 1].kind == kind;
}

static inline i32 pop_ctx(bc *bc, context_k kind) {
  while (bc->contexts[bc->ncontext - 1].kind != kind) {
    if (bc->ncontext == 0) {
      compiler_error(bc, GAB_PANIC,
                     "Internal compiler error: context stack underflow");
      return COMP_ERR;
      ;
    }

    bc->ncontext--;
  }

  bc->ncontext--;

  return COMP_OK;
}

static inline i32 push_ctx(bc *bc, context_k kind) {
  if (bc->ncontext == UINT8_MAX) {
    compiler_error(bc, GAB_PANIC, "Too many nested contexts");
    return COMP_ERR;
  }

  memset(bc->contexts + bc->ncontext, 0, sizeof(context));
  bc->contexts[bc->ncontext].kind = kind;

  return bc->ncontext++;
}

static inline i32 push_ctximpl(gab_engine *gab, bc *bc, u8 local) {
  i32 ctx = push_ctx(bc, kIMPL);

  if (ctx < 0)
    return COMP_ERR;

  context *c = bc->contexts + ctx;

  c->as.impl.local = local;
  c->as.impl.ctx = peek_ctx(bc, kFRAME, 0);

  return COMP_OK;
}

static inline i32 pop_ctximpl(bc *bc) {
  if (pop_ctx(bc, kIMPL) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);
  return COMP_OK;
}

static inline i32 peek_impl_frame(bc *bc) {
  i32 ctx = peek_ctx(bc, kIMPL, 0);

  if (ctx < 0)
    return COMP_ERR;

  return bc->contexts[ctx].as.impl.ctx;
}

static inline i32 peek_impl_local(bc *bc) {
  i32 ctx = peek_ctx(bc, kIMPL, 0);

  if (ctx < 0)
    return COMP_ERR;

  return bc->contexts[ctx].as.impl.local;
}

static inline boolean has_impl(bc *bc) {
  return peek_ctx(bc, kFRAME, 0) == peek_impl_frame(bc);
}

static gab_module *push_ctxframe(gab_engine *gab, bc *bc, gab_value name) {
  i32 ctx = push_ctx(bc, kFRAME);

  if (ctx < 0)
    return NULL;

  context *c = bc->contexts + ctx;

  frame *f = &c->as.frame;

  memset(f, 0, sizeof(frame));

  gab_module *mod = gab_module_create(gab, name, bc->lex.source);
  f->mod = mod;

  push_slot(bc, 1);

  initialize_local(bc, add_local(gab, bc, GAB_STRING("self"), 0));

  return mod;
}

static gab_obj_block_proto *pop_ctxframe(gab_engine *gab, bc *bc, boolean vse) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  u8 nupvalues = f->nupvalues;
  u8 nslots = f->nslots;
  u8 nargs = f->narguments;
  u8 nlocals = f->nlocals;

  gab_obj_block_proto *p =
      gab_obj_prototype_create(gab, f->mod, nargs, nslots, nlocals, nupvalues,
                               vse, f->upv_flags, f->upv_indexes);

  assert(match_ctx(bc, kFRAME));

  if (pop_ctx(bc, kFRAME) < 0)
    return NULL;

  return p;
}

gab_compile_rule get_rule(gab_token k);
i32 compile_exp_prec(gab_engine *gab, bc *bc, prec_k prec);
i32 compile_expression(gab_engine *gab, bc *bc);
i32 compile_tuple(gab_engine *gab, bc *bc, u8 want, boolean *vse_out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

static i32 compile_local(gab_engine *gab, bc *bc, gab_value name, u8 flags) {
  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  for (i32 local = f->next_local - 1; local >= 0; local--) {
    if (f->local_depths[local] != -1 &&
        f->local_depths[local] < f->scope_depth) {
      break;
    }

    gab_value other_local_name =
        v_gab_constant_val_at(&f->mod->constants, f->local_names[local]);

    if (name == other_local_name) {
      compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }
  }

  return add_local(gab, bc, name, flags);
}

i32 compile_parameters(gab_engine *gab, bc *bc, boolean *vse_out) {
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

    if (narguments >= GAB_ARG_MAX) {
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

      gab_value prop = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

      i32 local = compile_local(gab, bc, prop, 0);

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

  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;
  return narguments;
}

static inline i32 skip_newlines(bc *bc) {
  while (match_token(bc, TOKEN_NEWLINE)) {
    if (eat_token(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 optional_newline(bc *bc) {
  match_and_eat_token(bc, TOKEN_NEWLINE);
  return COMP_OK;
}

i32 compile_expressions_body(gab_engine *gab, bc *bc) {
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

i32 compile_expressions(gab_engine *gab, bc *bc) {
  push_scope(bc);

  u64 line = bc->line;

  if (compile_expressions_body(gab, bc) < 0)
    return COMP_ERR;

  pop_scope(bc);

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    compiler_error(bc, GAB_MISSING_END,
                   "Make sure the block at line %lu is closed.", line);
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_block_body(gab_engine *gab, bc *bc, u8 vse) {
  i32 result = compile_expressions(gab, bc);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_module_push_return(mod(bc), result, false, bc->lex.previous_token,
                         bc->line, bc->lex.previous_token_src);

  return COMP_OK;
}

i32 compile_message_spec(gab_engine *gab, bc *bc) {
  if (match_and_eat_token(bc, TOKEN_LBRACE)) {

    if (match_and_eat_token(bc, TOKEN_RBRACE)) {
      push_slot(bc, 1);
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
    push_load_local(bc, peek_impl_local(bc));
    return COMP_OK;
  }

  compiler_error(bc, GAB_MISSING_RECEIVER,
                 "Try either:\n\n\tSurrounding the definition with an "
                 "impl\n\t\timpl <receiver> ... end\n\n\tSpecifying a "
                 "receiver\n\t\tmsg[<receiver>] ... end");
  return COMP_ERR;
}

i32 compile_block(gab_engine *gab, bc *bc) {
  push_ctxframe(gab, bc, GAB_STRING("anonymous"));

  boolean vse;
  i32 narguments = compile_parameters(gab, bc, &vse);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc, vse) < 0)
    return COMP_ERR;

  gab_obj_block_proto *p = pop_ctxframe(gab, bc, vse);

  push_op(bc, OP_BLOCK);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(p)));

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_message(gab_engine *gab, bc *bc, gab_value name) {
  if (compile_message_spec(gab, bc) < 0)
    return COMP_ERR;

  push_ctxframe(gab, bc, name);

  boolean vse;
  i32 narguments = compile_parameters(gab, bc, &vse);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc, vse) < 0)
    return COMP_ERR;

  gab_obj_block_proto *p = pop_ctxframe(gab, bc, vse);

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_MESSAGE);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(p)));

  gab_obj_message *m = gab_obj_message_create(gab, name);
  u16 func_constant = add_constant(mod(bc), GAB_VAL_OBJ(m));
  push_short(bc, func_constant);

  push_slot(bc, 1);
  return COMP_OK;
}

i32 compile_expression(gab_engine *gab, bc *bc) {
  return compile_exp_prec(gab, bc, kASSIGNMENT);
}

i32 compile_tuple(gab_engine *gab, bc *bc, u8 want, boolean *vse_out) {
  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  u8 have = 0;
  boolean vse;

  i32 result;
  do {
    if (optional_newline(bc) < 0)
      return COMP_ERR;

    vse = false;
    result = compile_exp_prec(gab, bc, kASSIGNMENT);

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
      // We don't add the one because as far as the stack is concerned,
      // we have just filled the slots we wanted.
      push_slot(bc, want - have);
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

  assert(match_ctx(bc, kTUPLE));
  pop_ctx(bc, kTUPLE);

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

i32 compile_rec_tup_internal_item(gab_engine *gab, bc *bc, u8 index) {
  return compile_expression(gab, bc);
}

i32 compile_rec_tup_internals(gab_engine *gab, bc *bc, boolean *vse_out) {
  u8 size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_token(bc, TOKEN_RBRACE)) {

    i32 result = compile_rec_tup_internal_item(gab, bc, size);

    if (result < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    match_and_eat_token(bc, TOKEN_COMMA);

    if (skip_newlines(bc) < 0)
      return COMP_ERR;
    size++;
  }

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  boolean vse = gab_module_try_patch_vse(mod(bc), VAR_EXP);

  if (vse_out)
    *vse_out = vse;

  return size - vse;
}

i32 compile_assignment(gab_engine *gab, bc *bc, lvalue target) {
  boolean first = !match_ctx(bc, kASSIGNMENT_TARGET);

  if (first) {
    if (push_ctx(bc, kASSIGNMENT_TARGET) < 0)
      return COMP_ERR;
  }

  i32 ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

  if (ctx < 0)
    return COMP_ERR;

  v_lvalue *lvalues = &bc->contexts[ctx].as.assignment_target;

  v_lvalue_push(lvalues, target);

  /* A variable is declared in this assignment.
   * The slots of all previous lvalues must be corrected.
   */
  if (!first && target.kind == kNEW_LOCAL)
    for (i32 i = 0; i < lvalues->len; i++) {
      v_lvalue_ref_at(lvalues, i)->slot++;
    }

  if (!first)
    return COMP_OK;

  while (match_and_eat_token(bc, TOKEN_COMMA)) {
    if (compile_exp_prec(gab, bc, kASSIGNMENT) < 0)
      return COMP_ERR;
  }

  if (expect_token(bc, TOKEN_EQUAL) < 0)
    return COMP_ERR;

  gab_token prop_tok = bc->lex.previous_token;
  u64 prop_line = bc->line;
  s_i8 prop_src = bc->lex.previous_token_src;

  if (compile_tuple(gab, bc, lvalues->len, NULL) < 0)
    return COMP_ERR;

  for (u8 i = 0; i < lvalues->len; i++) {
    lvalue lval = v_lvalue_val_at(lvalues, lvalues->len - 1 - i);

    switch (lval.kind) {
    case kNEW_LOCAL:
      initialize_local(bc, lval.as.local);
    case kEXISTING_LOCAL:
      push_store_local(bc, lval.as.local);

      push_pop(bc, 1);
      pop_slot(bc, 1);
      break;
    case kINDEX:
    case kPROP: {
      u16 top = peek_slot(bc);

      u16 dist = top - lval.slot;

      push_shift(bc, dist);
    }
    }
  }

  for (u8 i = 0; i < lvalues->len; i++) {
    lvalue lval = v_lvalue_val_at(lvalues, lvalues->len - 1 - i);

    switch (lval.kind) {
    case kNEW_LOCAL:
    case kEXISTING_LOCAL:
      break;

    case kPROP: {
      gab_module_push_op(mod(bc), OP_STORE_PROPERTY_ANA, prop_tok, prop_line,
                         prop_src);

      gab_module_push_short(mod(bc), lval.as.property, prop_tok, prop_line,
                            prop_src);

      gab_module_push_inline_cache(mod(bc), prop_tok, prop_line, prop_src);

      push_pop(bc, 1);
      pop_slot(bc, 2);
      break;
    }

    case kINDEX: {
      u16 m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_SET));
      gab_module_push_send(mod(bc), 2, m, false, prop_tok, prop_line, prop_src);

      push_pop(bc, 1);
      pop_slot(bc, 3);
      break;
    }
    }
  }

  push_slot(bc, 1);
  push_op(bc, OP_PUSH_NIL);

  v_lvalue_destroy(lvalues);
  assert(match_ctx(bc, kASSIGNMENT_TARGET));
  pop_ctx(bc, kASSIGNMENT_TARGET);

  return COMP_OK;
}

i32 compile_var_decl(gab_engine *gab, bc *bc, u8 flags, u8 additional_local) {
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
      return COMP_ERR;
    }

    local_count++;

  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

initializer:
  push_slot(bc, local_count);

  switch (expect_token(bc, TOKEN_EQUAL)) {

  case COMP_OK:
    if (compile_tuple(gab, bc, local_count, NULL) < 0)
      return COMP_ERR;

    break;

  default:
    return COMP_ERR;
  }

  pop_slot(bc, local_count - 1);

  while (local_count--) {
    u8 local = locals[local_count];
    push_store_local(bc, local);
    initialize_local(bc, local);

    if (local_count != 0)
      push_pop(bc, 1);
  }

  return COMP_OK;
}

// Forward decl
i32 compile_definition(gab_engine *gab, bc *bc, s_i8 name, s_i8 help);

i32 compile_rec_internal_item(gab_engine *gab, bc *bc) {
  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {

    gab_obj_string *obj =
        gab_obj_string_create(gab, bc->lex.previous_token_src);

    gab_value val_name = GAB_VAL_OBJ(obj);

    push_slot(bc, 1);

    push_op(bc, OP_CONSTANT);
    push_short(bc, add_constant(mod(bc), val_name));

    switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

    case COMP_OK: {
      if (compile_expression(gab, bc) < 0)
        return COMP_ERR;

      return COMP_OK;
    }

    case COMP_TOKEN_NO_MATCH: {
      u8 value_in;
      i32 result = resolve_id(gab, bc, val_name, &value_in);

      push_slot(bc, 1);

      switch (result) {

      case COMP_RESOLVED_TO_LOCAL:
        push_load_local(bc, value_in);
        return COMP_OK;

      case COMP_RESOLVED_TO_UPVALUE:
        push_load_upvalue(bc, value_in);
        return COMP_OK;

      case COMP_ID_NOT_FOUND:
        push_op(bc, OP_PUSH_TRUE);
        return COMP_OK;

      default:
        goto err;
      }
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
  compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Try '<identifier>' or '[<exp>]'");
  return COMP_ERR;
}

i32 compile_rec_internals(gab_engine *gab, bc *bc) {
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

    match_and_eat_token(bc, TOKEN_COMMA);

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    size++;
  };

  return size;
}

i32 compile_record(gab_engine *gab, bc *bc) {
  i32 size = compile_rec_internals(gab, bc);

  if (size < 0)
    return COMP_ERR;

  push_op(bc, OP_RECORD);

  push_byte(bc, size);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_record_tuple(gab_engine *gab, bc *bc) {
  boolean vse;
  i32 size = compile_rec_tup_internals(gab, bc, &vse);

  if (size < 0)
    return COMP_ERR;

  gab_module_push_tuple(mod(bc), size, vse, bc->lex.previous_token, bc->line,
                        bc->lex.previous_token_src);

  pop_slot(bc, size);

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_definition(gab_engine *gab, bc *bc, s_i8 name, s_i8 help) {
  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

    u8 local = add_local(gab, bc, val_name, 0);

    push_slot(bc, 1);

    if (compile_record(gab, bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_TYPE);

    push_store_local(bc, local);

    initialize_local(bc, local);

    return COMP_OK;
  }

  // Const variable
  if (match_token(bc, TOKEN_EQUAL) || match_token(bc, TOKEN_COMMA)) {
    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

    u8 local = add_local(gab, bc, val_name, 0);

    if (match_and_eat_token(bc, TOKEN_COMMA) < 0)
      return COMP_ERR;

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

  // Create a local to store the new function in
  u8 local = add_local(gab, bc, val_name, 0);

  push_slot(bc, 1);

  // Now compile the impl
  if (compile_message(gab, bc, val_name) < 0)
    return COMP_ERR;

  push_store_local(bc, local);

  initialize_local(bc, local);

  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

i32 compile_exp_blk(gab_engine *gab, bc *bc, boolean assignable) {
  return compile_block(gab, bc);
}

i32 compile_exp_then(gab_engine *gab, bc *bc, boolean assignable) {
  u64 then_jump =
      gab_module_push_jump(mod(bc), OP_JUMP_IF_FALSE, bc->lex.previous_token,
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

i32 compile_exp_else(gab_engine *gab, bc *bc, boolean assignable) {
  u64 then_jump =
      gab_module_push_jump(mod(bc), OP_JUMP_IF_TRUE, bc->lex.previous_token,
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

i32 compile_exp_mch(gab_engine *gab, bc *bc, boolean assignable) {
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

    next = gab_module_push_jump(mod(bc), OP_POP_JUMP_IF_FALSE,
                                bc->lex.previous_token, bc->line,
                                bc->lex.previous_token_src);

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
               gab_module_push_jump(mod(bc), OP_JUMP, bc->lex.previous_token,
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

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_exp_bin(gab_engine *gab, bc *bc, boolean assignable) {
  gab_token op = bc->lex.previous_token;
  u64 line = bc->line;
  s_i8 src = bc->lex.previous_token_src;

  u16 m;

  switch (op) {
  case TOKEN_MINUS:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_SUB));
    break;

  case TOKEN_PLUS:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_ADD));
    break;

  case TOKEN_STAR:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_MUL));
    break;

  case TOKEN_SLASH:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_DIV));
    break;

  case TOKEN_PERCENT:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_MOD));
    break;

  case TOKEN_PIPE:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_BOR));
    break;

  case TOKEN_AMPERSAND:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_BND));
    break;

  case TOKEN_EQUAL_EQUAL:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_EQ));
    break;

  case TOKEN_LESSER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_LT));
    break;

  case TOKEN_LESSER_EQUAL:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_LTE));
    break;

  case TOKEN_LESSER_LESSER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_LSH));
    break;

  case TOKEN_GREATER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_GT));
    break;

  case TOKEN_GREATER_EQUAL:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_GTE));
    break;

  case TOKEN_GREATER_GREATER:
    m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_RSH));
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

i32 compile_exp_una(gab_engine *gab, bc *bc, boolean assignable) {
  gab_token op = bc->lex.previous_token;

  i32 result = compile_exp_prec(gab, bc, kUNARY);

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
a_i8 *parse_raw_str(bc *bc, s_i8 raw_str) {
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
i32 compile_exp_str(gab_engine *gab, bc *bc, boolean assignable) {
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

  push_slot(bc, 1);

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_itp(gab_engine *gab, bc *bc, boolean assignable) {
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

i32 compile_exp_grp(gab_engine *gab, bc *bc, boolean assignable) {
  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_num(gab_engine *gab, bc *bc, boolean assignable) {
  f64 num = strtod((char *)bc->lex.previous_token_src.data, NULL);
  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), GAB_VAL_NUMBER(num)));
  push_slot(bc, 1);
  return COMP_OK;
}

i32 compile_exp_bool(gab_engine *gab, bc *bc, boolean assignable) {
  push_op(bc,
          bc->lex.previous_token == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  push_slot(bc, 1);
  return COMP_OK;
}

i32 compile_exp_nil(gab_engine *gab, bc *bc, boolean assignable) {
  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);
  return COMP_OK;
}

i32 compile_exp_def(gab_engine *gab, bc *bc, boolean assignable) {
  s_i8 help = bc->lex.previous_comment;
  eat_token(bc);

  s_i8 name = {0};

  switch (bc->lex.previous_token) {
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

  if (compile_definition(gab, bc, name, help) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_arr(gab_engine *gab, bc *bc, boolean assignable) {
  return compile_record_tuple(gab, bc);
}

i32 compile_exp_rec(gab_engine *gab, bc *bc, boolean assignable) {
  return compile_record(gab, bc);
}

i32 compile_exp_imp(gab_engine *gab, bc *bc, boolean assignable) {
  push_scope(bc);

  if (!compile_expression(gab, bc))
    return COMP_ERR;

  i32 local = add_local(gab, bc, GAB_STRING(""), 0);

  if (local < 0)
    return COMP_ERR;

  initialize_local(bc, local);

  push_store_local(bc, local);

  push_pop(bc, 1);

  if (push_ctximpl(gab, bc, local) < 0)
    return COMP_ERR;

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  if (!expect_token(bc, TOKEN_END))
    return COMP_ERR;

  pop_ctximpl(bc);

  pop_scope(bc);

  return COMP_OK;
}

i32 compile_exp_ipm(gab_engine *gab, bc *bc, boolean assignable) {
  s_i8 offset = trim_prev_tok(bc);

  u32 local = strtoul((char *)offset.data, NULL, 10);

  if (local > GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (local - 1 >= f->narguments) {
    // There will always be at least one more local than arguments -
    //   The self local!
    if ((f->nlocals - f->narguments) > 1) {
      compiler_error(bc, GAB_INVALID_IMPLICIT, "");
      return COMP_ERR;
    }

    u8 missing_locals = local - f->narguments;

    f->narguments += missing_locals;

    push_slot(bc, missing_locals);

    for (i32 i = 0; i < missing_locals; i++) {
      i32 pad_local = compile_local(gab, bc, GAB_VAL_NUMBER(local - i), 0);

      if (pad_local < 0)
        return COMP_ERR;

      initialize_local(bc, pad_local);
    }
  }

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
      return COMP_ERR;
    }

    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                   "Parameters are not assignable, implicit or otherwise.");
    return COMP_ERR;
  }

  case COMP_TOKEN_NO_MATCH:
    push_load_local(bc, local);

    push_slot(bc, 1);

    break;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling 'implicit parameter' expression");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_idn(gab_engine *gab, bc *bc, boolean assignable) {
  gab_value id = prev_id(gab, bc);

  u8 index;
  i32 result = resolve_id(gab, bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      switch (result) {
      case COMP_ID_NOT_FOUND:
        index = compile_local(gab, bc, id, fMUTABLE);

        push_slot(bc, 1);

        return compile_assignment(gab, bc,
                                  (lvalue){
                                      .kind = kNEW_LOCAL,
                                      .slot = peek_slot(bc),
                                      .as.local = index,
                                  });

      case COMP_RESOLVED_TO_LOCAL: {
        i32 ctx = peek_ctx(bc, kFRAME, 0);
        frame *f = &bc->contexts[ctx].as.frame;

        if (!(f->local_flags[index] & fMUTABLE)) {
          compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                         "Cannot assign to immutable variable.");
          return COMP_ERR;
        }

        return compile_assignment(gab, bc,
                                  (lvalue){
                                      .kind = kEXISTING_LOCAL,
                                      .slot = peek_slot(bc),
                                      .as.local = index,
                                  });
      }
      case COMP_RESOLVED_TO_UPVALUE:
        compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                       "Captured variables are not assignable.");
        return COMP_ERR;
      }
    }
  }

  switch (result) {
  case COMP_RESOLVED_TO_LOCAL: {
    push_load_local(bc, index);
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    push_load_upvalue(bc, index);
    break;
  }

  case COMP_ID_NOT_FOUND: {
    compiler_error(bc, GAB_MISSING_IDENTIFIER,
                   "Double check the spelling of '%V'.", id);
    return COMP_ERR;
  }

  default:
    return COMP_ERR;
  }

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_exp_idx(gab_engine *gab, bc *bc, boolean assignable) {
  gab_token prop_tok = bc->lex.previous_token;
  u64 prop_line = bc->line;
  s_i8 prop_src = bc->lex.previous_token_src;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      return compile_assignment(gab, bc,
                                (lvalue){
                                    .kind = kINDEX,
                                    .slot = peek_slot(bc),
                                });
    }
  }

  u16 m = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_GET));
  gab_module_push_send(mod(bc), 1, m, false, prop_tok, prop_line, prop_src);

  pop_slot(bc, 2);
  push_slot(bc, 1);

  return VAR_EXP;
}

i32 compile_exp_dot(gab_engine *gab, bc *bc, boolean assignable) {
  s_i8 prop_name = trim_prev_tok(bc);

  i32 prop = add_string_constant(gab, mod(bc), prop_name);

  if (prop < 0)
    return COMP_ERR;

  gab_token prop_tok = bc->lex.previous_token;
  u64 prop_line = bc->line;
  s_i8 prop_src = bc->lex.previous_token_src;

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      return compile_assignment(gab, bc,
                                (lvalue){
                                    .kind = kPROP,
                                    .slot = peek_slot(bc),
                                    .as.property = prop,
                                });
    }
  }

  gab_module_push_op(mod(bc), OP_LOAD_PROPERTY_ANA, prop_tok, prop_line,
                     prop_src);

  gab_module_push_short(mod(bc), prop, prop_tok, prop_line, prop_src);

  gab_module_push_inline_cache(mod(bc), prop_tok, prop_line, prop_src);

  pop_slot(bc, 1);

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_arg_list(gab_engine *gab, bc *bc, boolean *vse_out) {
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

#define fHAS_PAREN 1
#define fHAS_BRACK 2
#define fHAS_DO 4

i32 compile_arguments(gab_engine *gab, bc *bc, boolean *vse_out, u8 flags) {
  i32 result = 0;
  *vse_out = false;

  if (flags & fHAS_PAREN ||
      (~flags & fHAS_DO && match_and_eat_token(bc, TOKEN_LPAREN))) {
    // Normal function args
    result = compile_arg_list(gab, bc, vse_out);
    if (result < 0)
      return COMP_ERR;
  }

  if (flags & fHAS_BRACK || match_and_eat_token(bc, TOKEN_LBRACK)) {
    // record argument
    if (compile_record(gab, bc) < 0)
      return COMP_ERR;

    result += 1 + *vse_out;
    *vse_out = false;
  }

  if (flags & fHAS_DO || match_and_eat_token(bc, TOKEN_DO)) {
    // We are an anonyumous function
    if (compile_block(gab, bc) < 0)
      return COMP_ERR;

    result += 1 + *vse_out;
    *vse_out = false;
  }

  return result;
}

i32 compile_exp_emp(gab_engine *gab, bc *bc, boolean assignable) {
  s_i8 message = bc->lex.previous_token_src;
  gab_token tok = bc->lex.previous_token;
  u64 line = bc->line;

  gab_value val_name =
      GAB_VAL_OBJ(gab_obj_string_create(gab, trim_prev_tok(bc)));

  u16 m = add_message_constant(gab, mod(bc), val_name);

  push_op(bc, OP_PUSH_NIL);

  push_slot(bc, 1);

  boolean vse;
  i32 result = compile_arguments(gab, bc, &vse, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_send(mod(bc), result, m, vse, tok, line, message);

  pop_slot(bc, result);

  return VAR_EXP;
}

i32 compile_exp_amp(gab_engine *gab, bc *bc, boolean assignable) {
  if (match_and_eat_token(bc, TOKEN_MESSAGE)) {
    s_i8 message = bc->lex.previous_token_src;

    gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, message));

    u16 f = add_message_constant(gab, mod(bc), val_name);

    push_op(bc, OP_CONSTANT);
    push_short(bc, f);

    push_slot(bc, 1);

    return COMP_OK;
  }

  const char *msg;
  eat_token(bc);
  switch (bc->lex.previous_token) {
  case TOKEN_EQUAL_EQUAL:
    msg = mGAB_EQ;
    break;
  case TOKEN_STAR:
    msg = mGAB_MUL;
    break;
  case TOKEN_SLASH:
    msg = mGAB_DIV;
    break;
  case TOKEN_PLUS:
    msg = mGAB_ADD;
    break;
  case TOKEN_MINUS:
    msg = mGAB_SUB;
    break;
  case TOKEN_PERCENT:
    msg = mGAB_MOD;
    break;
  case TOKEN_GREATER_GREATER:
    msg = mGAB_RSH;
    break;
  case TOKEN_GREATER_EQUAL:
    msg = mGAB_GTE;
    break;
  case TOKEN_GREATER:
    msg = mGAB_GT;
    break;
  case TOKEN_LESSER_LESSER:
    msg = mGAB_LSH;
    break;
  case TOKEN_LESSER_EQUAL:
    msg = mGAB_LTE;
    break;
  case TOKEN_LESSER:
    msg = mGAB_LT;
    break;
  case TOKEN_PIPE:
    msg = mGAB_BOR;
    break;
  case TOKEN_AMPERSAND:
    msg = mGAB_BND;
    break;
  case TOKEN_LPAREN:
    expect_token(bc, TOKEN_RPAREN);
    msg = mGAB_CALL;
    break;
  case TOKEN_LBRACE:
    if (match_and_eat_token(bc, TOKEN_EQUAL))
      msg = mGAB_SET;
    else
      msg = mGAB_GET;
    expect_token(bc, TOKEN_RBRACE);
    break;
  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected a message literal.");
    return COMP_ERR;
  }

  u16 f = add_message_constant(gab, mod(bc), GAB_STRING(msg));

  push_op(bc, OP_CONSTANT);
  push_short(bc, f);

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_exp_snd(gab_engine *gab, bc *bc, boolean assignable) {
  s_i8 prev_src = bc->lex.previous_token_src;
  gab_token prev_tok = bc->lex.previous_token;
  u64 prev_line = bc->line;

  s_i8 message = trim_prev_tok(bc);

  gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, message));

  u16 m = add_message_constant(gab, mod(bc), val_name);

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  gab_module_push_send(mod(bc), result, m, vse, prev_tok, prev_line, prev_src);

  push_slot(bc, 1);

  return VAR_EXP;
}

i32 compile_exp_cal(gab_engine *gab, bc *bc, boolean assignable) {
  gab_token call_tok = bc->lex.previous_token;
  u64 call_line = bc->line;
  s_i8 call_src = bc->lex.previous_token_src;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, fHAS_PAREN);

  if (result < 0)
    return COMP_ERR;

  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  u16 msg = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_CALL));

  gab_module_push_send(mod(bc), result, msg, vse, call_tok, call_line,
                       call_src);

  push_slot(bc, 1);

  return VAR_EXP;
}

i32 compile_exp_bcal(gab_engine *gab, bc *bc, boolean assignable) {
  gab_token call_tok = bc->lex.previous_token;
  u64 call_line = bc->line;
  s_i8 call_src = bc->lex.previous_token_src;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, fHAS_DO);

  if (result < 0)
    return COMP_ERR;
  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }
  pop_slot(bc, result);
  u16 msg = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_CALL));
  gab_module_push_send(mod(bc), result, msg, vse, call_tok, call_line,
                       call_src);
  return VAR_EXP;
}

i32 compile_exp_rcal(gab_engine *gab, bc *bc, boolean assignable) {
  gab_token call_tok = bc->lex.previous_token;
  u64 call_line = bc->line;
  s_i8 call_src = bc->lex.previous_token_src;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, &vse, fHAS_BRACK);

  pop_slot(bc, 1);

  u16 msg = add_message_constant(gab, mod(bc), GAB_STRING(mGAB_CALL));

  gab_module_push_send(mod(bc), result, msg, vse, call_tok, call_line,
                       call_src);

  return VAR_EXP;
}

i32 compile_exp_and(gab_engine *gab, bc *bc, boolean assignable) {
  u64 end_jump =
      gab_module_push_jump(mod(bc), OP_LOGICAL_AND, bc->lex.previous_token,
                           bc->line, bc->lex.previous_token_src);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, kAND) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

i32 compile_exp_or(gab_engine *gab, bc *bc, boolean assignable) {
  u64 end_jump =
      gab_module_push_jump(mod(bc), OP_LOGICAL_OR, bc->lex.previous_token,
                           bc->line, bc->lex.previous_token_src);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, kOR) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

i32 compile_exp_prec(gab_engine *gab, bc *bc, prec_k prec) {
  if (eat_token(bc) < 0)
    return COMP_ERR;

  gab_compile_rule rule = get_rule(bc->lex.previous_token);

  if (rule.prefix == NULL) {
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression.");
    return COMP_ERR;
  }

  boolean assignable = prec <= kASSIGNMENT;

  i32 have = rule.prefix(gab, bc, assignable);
  if (have < 0)
    return COMP_ERR;

  while (prec <= get_rule(bc->lex.current_token).prec) {
    if (have < 0)
      return COMP_ERR;

    if (eat_token(bc) < 0)
      return COMP_ERR;

    rule = get_rule(bc->lex.previous_token);

    if (rule.infix != NULL) {
      // Treat this as an infix expression.
      have = rule.infix(gab, bc, assignable);
    }
  }

  if (!assignable && match_token(bc, TOKEN_EQUAL)) {
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  return have;
}

i32 compile_exp_for(gab_engine *gab, bc *bc, boolean assignable) {
  push_scope(bc);

  gab_token prev_tok = bc->lex.previous_token;
  u64 prev_line = bc->line;
  s_i8 prev_src = bc->lex.previous_token_src;

  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  u8 local_start = f->next_local;
  u8 nlooplocals = 0;
  i32 result;

  boolean var = false;

  do {
    if (var)
      break; // If we encountered a var param, break out of this loop.

    switch (match_and_eat_token(bc, TOKEN_DOT_DOT)) {
    case COMP_OK:
      var = true;
      // Fallthrough
    case COMP_TOKEN_NO_MATCH: {
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      s_i8 name = bc->lex.previous_token_src;

      gab_value val_name = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

      i32 loc = compile_local(gab, bc, val_name, 0);

      if (loc < 0)
        return COMP_ERR;

      initialize_local(bc, loc);
      nlooplocals++;
      break;
    }

    default:
      return COMP_ERR;
    }
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

  initialize_local(bc, add_local(gab, bc, GAB_STRING(""), 0));

  push_slot(bc, nlooplocals + 1);

  if (expect_token(bc, TOKEN_IN) < 0)
    return COMP_ERR;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  gab_module_try_patch_vse(mod(bc), VAR_EXP);

  pop_slot(bc, 1);

  u64 loop = gab_module_push_loop(mod(bc));

  if (var)
    gab_module_push_pack(mod(bc), nlooplocals - 1, 1, prev_tok, prev_line,
                         prev_src);

  u64 jump_start = gab_module_push_iter(mod(bc), local_start, nlooplocals,
                                        prev_tok, prev_line, prev_src);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_module_push_next(mod(bc), local_start + nlooplocals, prev_tok, prev_line,
                       prev_src);

  pop_slot(bc, 1);

  gab_module_patch_loop(mod(bc), loop, prev_tok, prev_line, prev_src);

  gab_module_patch_jump(mod(bc), jump_start);

  pop_scope(bc);

  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_exp_lop(gab_engine *gab, bc *bc, boolean assignable) {
  push_scope(bc);

  gab_token prev_tok = bc->lex.previous_token;
  u64 prev_line = bc->line;
  s_i8 prev_src = bc->lex.previous_token_src;

  u64 loop = gab_module_push_loop(mod(bc));

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (match_and_eat_token(bc, TOKEN_UNTIL)) {
    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    prev_tok = bc->lex.previous_token;
    prev_line = bc->line;
    prev_src = bc->lex.previous_token_src;

    u64 jump = gab_module_push_jump(mod(bc), OP_POP_JUMP_IF_TRUE, prev_tok,
                                    prev_line, prev_src);

    gab_module_patch_loop(mod(bc), loop, prev_tok, prev_line, prev_src);

    gab_module_patch_jump(mod(bc), jump);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    gab_module_patch_loop(mod(bc), loop, prev_tok, prev_line, prev_src);
  }

  push_op(bc, OP_PUSH_NIL);

  pop_scope(bc);
  return COMP_OK;
}

i32 compile_exp_sym(gab_engine *gab, bc *bc, boolean assignable) {
  s_i8 name = trim_prev_tok(bc);

  gab_value sym = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), GAB_VAL_OBJ(sym)));

  push_slot(bc, 1);

  return COMP_OK;
}

i32 compile_exp_yld(gab_engine *gab, bc *bc, boolean assignable) {

  if (!get_rule(bc->lex.current_token).prefix) {
    gab_obj_suspense_proto *proto =
        gab_obj_suspense_proto_create(gab, mod(bc)->bytecode.len + 4, 1);

    u16 kproto = add_constant(mod(bc), GAB_VAL_OBJ(proto));

    gab_module_push_yield(mod(bc), kproto, 0, false, bc->lex.previous_token,
                          bc->line, bc->lex.previous_token_src);

    push_slot(bc, 1);

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

  gab_obj_suspense_proto *proto =
      gab_obj_suspense_proto_create(gab, mod(bc)->bytecode.len + 4, 1);

  u16 kproto = add_constant(mod(bc), GAB_VAL_OBJ(proto));

  push_slot(bc, 1);

  gab_module_push_yield(mod(bc), kproto, result, vse, bc->lex.previous_token,
                        bc->line, bc->lex.previous_token_src);

  pop_slot(bc, result);

  return VAR_EXP;
}

i32 compile_exp_rtn(gab_engine *gab, bc *bc, boolean assignable) {
  if (!get_rule(bc->lex.current_token).prefix) {
    push_slot(bc, 1);

    push_op(bc, OP_PUSH_NIL);

    gab_module_push_return(mod(bc), 1, false, bc->lex.previous_token, bc->line,
                           bc->lex.previous_token_src);

    pop_slot(bc, 1);
    return COMP_OK;
  }

  boolean vse;
  i32 result = compile_tuple(gab, bc, VAR_EXP, &vse);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_RET_MAX) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_module_push_return(mod(bc), result, vse, bc->lex.previous_token, bc->line,
                         bc->lex.previous_token_src);

  pop_slot(bc, result);
  return COMP_OK;
}

// All of the expression compiling functions follow the naming convention
// compile_exp_XXX.
#define NONE()                                                                 \
  { NULL, NULL, kNONE, false }
#define PREFIX(fnc)                                                            \
  { compile_exp_##fnc, NULL, kNONE, false }
#define INFIX(fnc, prec, is_multi)                                             \
  { NULL, compile_exp_##fnc, k##prec, is_multi }
#define PREFIX_INFIX(pre, in, prec, is_multi)                                  \
  { compile_exp_##pre, compile_exp_##in, k##prec, is_multi }

// ----------------Pratt Parsing Table ----------------------
const gab_compile_rule gab_bc_rules[] = {
    INFIX(then, AND, false),                        // THEN
    INFIX(else, OR, false),                            // ELSE
    PREFIX_INFIX(blk, bcal, SEND, false),                       // DO
    PREFIX(for),                       // FOR
    INFIX(mch, MATCH, false),                       //MATCH 
    NONE(),                            // IN
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
    PREFIX(ipm), //IMPLICIT
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

gab_value compile(gab_engine *gab, bc *bc, gab_value name, u8 narguments,
                  gab_value arguments[narguments]) {
  gab_module *new_mod = push_ctxframe(gab, bc, name);

  i32 ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;

  if (eat_token(bc) == COMP_ERR)
    return GAB_VAL_UNDEFINED();

  if (bc->lex.current_token == TOKEN_EOF)
    return GAB_VAL_UNDEFINED();

  push_slot(bc, narguments);

  for (u8 i = 0; i < narguments; i++) {
    initialize_local(bc, add_local(gab, bc, arguments[i], 0));
  }

  if (compile_expressions_body(gab, bc) < 0)
    return GAB_VAL_UNDEFINED();

  gab_module_push_return(mod(bc), 1, false, bc->lex.previous_token, bc->line,
                         bc->lex.previous_token_src);

  gab_obj_block_proto *p = pop_ctxframe(gab, bc, false);

  add_constant(new_mod, GAB_VAL_OBJ(p));

  gab_obj_block *main = gab_obj_block_create(gab, p);

  add_constant(new_mod, GAB_VAL_OBJ(main));

  if (fGAB_DUMP_BYTECODE & bc->flags) {
    gab_dis(new_mod);
  }

  return GAB_VAL_OBJ(main);
}

gab_value gab_bc_compile_send(gab_engine *gab, gab_value msg,
                              gab_value receiver, u8 flags, u8 narguments,
                              gab_value arguments[narguments]) {
  gab_module *mod = gab_module_create(gab, GAB_STRING("send"), NULL);

  u16 message = add_constant(mod, msg);

  u16 constant = add_constant(mod, receiver);

  gab_module_push_op(mod, OP_CONSTANT, 0, 0, (s_i8){0});
  gab_module_push_short(mod, constant, 0, 0, (s_i8){0});

  for (u8 i = 0; i < narguments; i++) {
    u16 constant = add_constant(mod, arguments[i]);

    gab_module_push_op(mod, OP_CONSTANT, 0, 0, (s_i8){0});
    gab_module_push_short(mod, constant, 0, 0, (s_i8){0});
  }

  gab_module_push_send(mod, narguments, message, false, 0, 0, (s_i8){0});

  gab_module_push_return(mod, 1, false, 0, 0, (s_i8){0});

  u8 nlocals = narguments + 1;

  gab_obj_block_proto *p = gab_obj_prototype_create(
      gab, mod, narguments, nlocals, nlocals, 0, false, NULL, NULL);

  add_constant(mod, GAB_VAL_OBJ(p));

  gab_obj_block *main = gab_obj_block_create(gab, p);

  return GAB_VAL_OBJ(main);
}

gab_value gab_bc_compile(gab_engine *gab, gab_value name, s_i8 source, u8 flags,
                         u8 narguments, gab_value arguments[narguments]) {
  bc bc;
  gab_bc_create(&bc, gab_source_create(gab, source), flags);

  gab_value module = compile(gab, &bc, name, narguments, arguments);

  return module;
}

static void compiler_error(bc *bc, gab_status e, const char *help_fmt, ...) {
  if (bc->panic) {
    return;
  }

  bc->panic = true;

  if (bc->flags & fGAB_DUMP_ERROR) {
    gab_source *src = mod(bc)->source;

    va_list args;
    va_start(args, help_fmt);

    gab_lexer_finish_line(&bc->lex);

    i32 ctx = peek_ctx(bc, kFRAME, 0);
    frame *f = &bc->contexts[ctx].as.frame;

    s_i8 curr_token = bc->lex.previous_token_src;
    u64 curr_src_index = bc->line - 1;
    s_i8 curr_src;

    if (curr_src_index < src->source->len) {
      curr_src = v_s_i8_val_at(&src->source_lines, curr_src_index);
    } else {
      curr_src = s_i8_cstr("");
    }

    const i8 *curr_src_start = curr_src.data;
    i32 curr_src_len = curr_src.len;

    while (is_whitespace(*curr_src_start)) {
      curr_src_start++;
      curr_src_len--;
      if (curr_src_len == 0)
        break;
    }

    gab_value func_name =
        v_gab_constant_val_at(&f->mod->constants, f->mod->name);

    a_i8 *curr_under = a_i8_empty(curr_src_len);

    const i8 *tok_start, *tok_end;

    tok_start = curr_token.data;
    tok_end = curr_token.data + curr_token.len;

    const char *tok = gab_token_names[bc->lex.previous_token];

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
            (i32)curr_src_len, curr_src_start, (i32)curr_under->len,
            curr_under->data);

    a_i8_destroy(curr_under);

    fprintf(stderr,
            ANSI_COLOR_YELLOW "%s. \n\n" ANSI_COLOR_RESET "\t" ANSI_COLOR_GREEN,
            gab_status_names[e]);

    vfprintf(stderr, help_fmt, args);

    fprintf(stderr, "\n" ANSI_COLOR_RESET);

    va_end(args);
  }
}
