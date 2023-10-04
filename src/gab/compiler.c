#include "include/compiler.h"
#include "include/char.h"
#include "include/colors.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/lexer.h"
#include "include/module.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  struct gab_mod *mod;

  int scope_depth;

  uint8_t narguments;

  uint16_t next_slot;
  uint16_t nslots;

  uint8_t next_local;
  uint8_t nlocals;

  uint8_t nupvalues;

  uint8_t local_flags[GAB_LOCAL_MAX];
  int local_depths[GAB_LOCAL_MAX];
  gab_value local_names[GAB_LOCAL_MAX];

  uint8_t upv_flags[GAB_UPVALUE_MAX];
  uint8_t upv_indexes[GAB_UPVALUE_MAX];
} frame;

typedef enum {
  kNEW_LOCAL,
  kEXISTING_LOCAL,
  kNEW_REST_LOCAL,
  kEXISTING_REST_LOCAL,
  kPROP,
  kINDEX,
} lvalue_k;

typedef struct {
  lvalue_k kind;

  uint16_t slot;

  union {
    uint16_t local;
    uint16_t property;
  } as;

} lvalue;

#define T lvalue
#include "include/vector.h"

#define T uint16_t
#include "include/vector.h"

typedef enum {
  kIMPL,
  kFRAME,
  kTUPLE,
  kASSIGNMENT_TARGET,
  kMATCH_TARGET,
  kLOOP,
} context_k;

typedef struct {
  context_k kind;

  union {
    v_lvalue assignment_target;

    frame frame;
  } as;
} context;

typedef struct {
  struct gab_src *source;

  size_t offset;

  uint8_t flags;

  bool panic;

  uint8_t ncontext;
  context contexts[cGAB_FUNCTION_DEF_NESTING_MAX];
} bc;

static const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
};

/*
  Precedence rules for the parsing of expressions.
*/
typedef enum prec_k {
  kNONE,
  kIN,
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

typedef int (*gab_compile_func)(struct gab_eg *, bc *, bool);
typedef struct gab_compile_rule compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  prec_k prec;
  bool multi_line;
};

void gab_bc_create(bc *self, struct gab_src *source, uint8_t flags) {
  memset(self, 0, sizeof(*self));

  self->flags = flags;
  self->source = source;
}

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

static bool match_token(bc *bc, gab_token tok);

static int eat_token(bc *bc);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline bool match_token(bc *bc, gab_token tok) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset) == tok;
}

static inline bool match_terminator(bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL) || match_token(bc, TOKEN_EOF);
}

static int eat_token(bc *bc) {
  bc->offset++;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    compiler_error(bc, GAB_MALFORMED_TOKEN, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

static inline int match_and_eat_token(bc *bc, gab_token tok) {
  if (!match_token(bc, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

static inline int expect_token(bc *bc, gab_token tok) {
  if (!match_token(bc, tok)) {
    eat_token(bc);
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "Expected " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET,
                   gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(bc);
}

static inline int match_tokoneof(bc *bc, gab_token toka, gab_token tokb) {
  return match_token(bc, toka) || match_token(bc, tokb);
}

static inline int match_and_eat_tokoneof(bc *bc, gab_token toka,
                                         gab_token tokb) {
  if (!match_tokoneof(bc, toka, tokb))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

size_t prev_line(bc *bc) {
  return v_uint64_t_val_at(&bc->source->tokens_line, bc->offset - 1);
}

gab_token curr_tok(bc *bc) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset);
}

gab_token prev_tok(bc *bc) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset - 1);
}

s_char prev_src(bc *bc) {
  return v_s_char_val_at(&bc->source->tokens_src, bc->offset - 1);
}

s_char trim_prev_src(bc *bc) {
  s_char message = prev_src(bc);
  // SKip the ':' at the beginning
  message.data++;
  message.len--;

  return message;
}

gab_value prev_id(struct gab_eg *gab, bc *bc) {
  s_char s = prev_src(bc);
  return gab_nstring(gab, s.len, s.data);
}

static inline int peek_ctx(bc *bc, context_k kind, uint8_t depth) {
  int idx = bc->ncontext - 1;

  while (idx >= 0) {
    if (bc->contexts[idx].kind == kind)
      if (depth-- == 0)
        return idx;

    idx--;
  }

  return COMP_CONTEXT_NOT_FOUND;
}

static inline struct gab_mod *mod(bc *bc) {
  int frame = peek_ctx(bc, kFRAME, 0);
  return bc->contexts[frame].as.frame.mod;
}

static inline uint16_t add_constant(struct gab_mod *mod, gab_value value) {
  return gab_mod_add_constant(mod, value);
}

static inline void push_op(bc *bc, gab_opcode op) {
  gab_mod_push_op(mod(bc), op, prev_tok(bc), prev_line(bc), prev_src(bc));
}

static inline void push_byte(bc *bc, uint8_t data) {
  gab_mod_push_byte(mod(bc), data, prev_tok(bc), prev_line(bc), prev_src(bc));
}

static inline void push_shift(bc *bc, uint8_t n) {
  if (n <= 1)
    return;

  push_op(bc, OP_SHIFT);

  push_byte(bc, n);
}

static inline void push_pop(bc *bc, uint8_t n) {
  gab_mod_push_pop(mod(bc), n, prev_tok(bc), prev_line(bc), prev_src(bc));
}

static inline void push_store_local(bc *bc, uint8_t local) {
  gab_mod_push_store_local(mod(bc), local, prev_tok(bc), prev_line(bc),
                           prev_src(bc));
}

static inline void push_load_local(bc *bc, uint8_t local) {
  gab_mod_push_load_local(mod(bc), local, prev_tok(bc), prev_line(bc),
                          prev_src(bc));
}

static inline void push_load_upvalue(bc *bc, uint8_t upv) {
  gab_mod_push_load_upvalue(mod(bc), upv, prev_tok(bc), prev_line(bc),
                            prev_src(bc));
}

static inline void push_short(bc *bc, uint16_t data) {
  gab_mod_push_short(mod(bc), data, prev_tok(bc), prev_line(bc), prev_src(bc));
}

static inline uint16_t peek_slot(bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  return f->next_slot;
}

static inline void push_slot(bc *bc, uint16_t n) {
  int ctx = peek_ctx(bc, kFRAME, 0);
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

static inline void pop_slot(bc *bc, uint16_t n) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot - n < 0) {
    compiler_error(bc, GAB_PANIC,
                   "Too few slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot -= n;
}

static void initialize_local(bc *bc, uint8_t local) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->local_depths[local] != -1)
    return;

  f->local_depths[local] = f->scope_depth;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static int add_local(struct gab_eg *gab, bc *bc, gab_value name,
                     uint8_t flags) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->nlocals == GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  uint8_t local = f->next_local;
  f->local_names[local] = name;
  f->local_depths[local] = -1;
  f->local_flags[local] = flags;

  if (++f->next_local > f->nlocals)
    f->nlocals = f->next_local;

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * upvalue.
 */
static int add_upvalue(bc *bc, int depth, uint8_t index, uint8_t flags) {
  int ctx = peek_ctx(bc, kFRAME, depth);

  frame *f = &bc->contexts[ctx].as.frame;
  uint16_t count = f->nupvalues;

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
static int resolve_local(struct gab_eg *gab, bc *bc, gab_value name,
                         uint8_t depth) {
  int ctx = peek_ctx(bc, kFRAME, depth);

  if (ctx < 0) {
    return COMP_LOCAL_NOT_FOUND;
  }

  frame *f = &bc->contexts[ctx].as.frame;

  for (int local = f->next_local - 1; local >= 0; local--) {
    gab_value other_local_name = f->local_names[local];

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
static int resolve_upvalue(struct gab_eg *gab, bc *bc, gab_value name,
                           uint8_t depth) {
  if (depth >= bc->ncontext) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  int local = resolve_local(gab, bc, name, depth + 1);

  if (local >= 0) {
    int ctx = peek_ctx(bc, kFRAME, depth + 1);
    frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->local_flags[local] |= fVAR_CAPTURED;

    if (flags & fVAR_MUTABLE) {
      compiler_error(bc, GAB_CAPTURED_MUTABLE, "");
      return COMP_ERR;
    }

    return add_upvalue(bc, depth, local, flags | fVAR_LOCAL);
  }

  int upvalue = resolve_upvalue(gab, bc, name, depth + 1);
  if (upvalue >= 0) {
    int ctx = peek_ctx(bc, kFRAME, depth + 1);
    frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->upv_flags[upvalue];
    return add_upvalue(bc, depth, upvalue, flags & ~fVAR_LOCAL);
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_RESOLVED_TO_LOCAL if the id was a local, and
 * COMP_RESOLVED_TO_UPVALUE if the id was an upvalue.
 */
static int resolve_id(struct gab_eg *gab, bc *bc, gab_value name,
                      uint8_t *value_in) {
  int arg = resolve_local(gab, bc, name, 0);

  if (arg == COMP_ERR)
    return arg;

  if (arg == COMP_LOCAL_NOT_FOUND) {

    arg = resolve_upvalue(gab, bc, name, 0);
    if (arg == COMP_ERR)
      return arg;

    if (arg == COMP_UPVALUE_NOT_FOUND)
      return COMP_ID_NOT_FOUND;

    if (value_in)
      *value_in = (uint8_t)arg;
    return COMP_RESOLVED_TO_UPVALUE;
  }

  if (value_in)
    *value_in = (uint8_t)arg;
  return COMP_RESOLVED_TO_LOCAL;
}

// static inline int peek_scope(bc *bc) {
//   int ctx = peek_ctx(bc, kFRAME, 0);
//   frame *f = &bc->contexts[ctx].as.frame;
//   return f->scope_depth;
// }

static void push_scope(bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;
  f->scope_depth++;
}

static void pop_scope(bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->scope_depth--;

  uint8_t slots = 0;
  while (f->next_local > 1) {
    uint8_t local = f->next_local - 1;

    if (f->local_depths[local] <= f->scope_depth)
      break;

    f->next_local--;
    slots++;
  }

  pop_slot(bc, slots);

  push_op(bc, OP_DROP);
  push_byte(bc, slots);
}

static inline bool match_ctx(bc *bc, context_k kind) {
  return bc->contexts[bc->ncontext - 1].kind == kind;
}

static inline int pop_ctx(bc *bc, context_k kind) {
  while (bc->contexts[bc->ncontext - 1].kind != kind) {
    if (bc->ncontext == 0) {
      compiler_error(bc, GAB_PANIC,
                     "Internal compiler error: context stack underflow");
      return COMP_ERR;
    }

    bc->ncontext--;
  }

  bc->ncontext--;

  return COMP_OK;
}

static inline int push_ctx(bc *bc, context_k kind) {
  if (bc->ncontext == UINT8_MAX) {
    compiler_error(bc, GAB_PANIC, "Too many nested contexts");
    return COMP_ERR;
  }

  memset(bc->contexts + bc->ncontext, 0, sizeof(context));
  bc->contexts[bc->ncontext].kind = kind;

  return bc->ncontext++;
}

static struct gab_mod *push_ctxframe(struct gab_eg *gab, bc *bc,
                                     gab_value name) {
  int ctx = push_ctx(bc, kFRAME);

  if (ctx < 0)
    return NULL;

  context *c = bc->contexts + ctx;

  frame *f = &c->as.frame;

  memset(f, 0, sizeof(frame));

  struct gab_mod *mod = gab_mod(gab, name, bc->source);
  f->mod = mod;

  push_slot(bc, 1);

  initialize_local(bc, add_local(gab, bc, gab_string(gab, "self"), 0));

  return mod;
}

static struct gab_obj_block_proto *pop_ctxframe(struct gab_eg *gab, bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  uint8_t nupvalues = f->nupvalues;
  uint8_t nslots = f->nslots;
  uint8_t nargs = f->narguments;
  uint8_t nlocals = f->nlocals;

  gab_value p = gab_blkproto(gab, f->mod, nargs, nslots, nlocals, nupvalues,
                             f->upv_flags, f->upv_indexes);

  assert(match_ctx(bc, kFRAME));

  if (pop_ctx(bc, kFRAME) < 0)
    return NULL;

  return GAB_VAL_TO_BLOCK_PROTO(p);
}

compile_rule get_rule(gab_token k);
int compile_exp_prec(struct gab_eg *gab, bc *bc, prec_k prec);
int compile_expression(struct gab_eg *gab, bc *bc);
int compile_tuple(struct gab_eg *gab, bc *bc, uint8_t want, bool *mv_out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

static int compile_local(struct gab_eg *gab, bc *bc, gab_value name,
                         uint8_t flags) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  for (int local = f->next_local - 1; local >= 0; local--) {
    if (f->local_depths[local] != -1 &&
        f->local_depths[local] < f->scope_depth) {
      break;
    }

    gab_value other_local_name = f->local_names[local];

    if (name == other_local_name) {
      compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }
  }

  return add_local(gab, bc, name, flags);
}

int compile_parameters(struct gab_eg *gab, bc *bc) {
  int mv = -1;
  int result = 0;
  uint8_t narguments = 0;

  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  if (!match_and_eat_token(bc, TOKEN_LPAREN))
    goto fin;

  if (match_and_eat_token(bc, TOKEN_RPAREN) > 0)
    goto fin;

  do {
    if (narguments >= GAB_ARG_MAX) {
      compiler_error(bc, GAB_TOO_MANY_PARAMETERS, "");
      return COMP_ERR;
    }

    narguments++;

    switch (match_and_eat_token(bc, TOKEN_DOT_DOT)) {
    case COMP_OK:
      if (mv >= 0) {
        compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                       "Multiple variadic parameters");
        return COMP_ERR;
      }

      narguments--;
      mv = narguments;
      // falthrough

    case COMP_TOKEN_NO_MATCH: {
      // This is a normal paramter
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      s_char name = prev_src(bc);

      gab_value val_name = gab_nstring(gab, name.len, name.data);

      int local = compile_local(gab, bc, val_name, 0);

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

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  if (pop_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  if (mv >= 0)
    gab_mod_push_pack(mod(bc), mv, narguments - mv, prev_tok(bc), prev_line(bc),
                      prev_src(bc));

  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = mv >= 0 ? VAR_EXP : narguments;
  return f->narguments;
}

static inline int skip_newlines(bc *bc) {
  while (match_token(bc, TOKEN_NEWLINE)) {
    if (eat_token(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline int optional_newline(bc *bc) {
  match_and_eat_token(bc, TOKEN_NEWLINE);
  return COMP_OK;
}

int compile_expressions_body(struct gab_eg *gab, bc *bc) {
  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (match_terminator(bc))
    return COMP_OK;

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
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

    if (expect_token(bc, TOKEN_NEWLINE) < 0)
      return COMP_ERR;

    if (skip_newlines(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

int compile_expressions(struct gab_eg *gab, bc *bc) {
  push_scope(bc);

  uint64_t line = prev_line(bc);

  if (compile_expressions_body(gab, bc) < 0)
    return COMP_ERR;

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    compiler_error(bc, GAB_MISSING_END,
                   "Make sure the block at line %lu is closed.", line);
    return COMP_ERR;
  }

  pop_scope(bc);

  return COMP_OK;
}

int compile_block_body(struct gab_eg *gab, bc *bc) {
  int result = compile_expressions(gab, bc);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_push_return(mod(bc), result, false, prev_tok(bc), prev_line(bc),
                      prev_src(bc));

  return COMP_OK;
}

int compile_message_spec(struct gab_eg *gab, bc *bc, gab_value name) {
  if (expect_token(bc, TOKEN_LBRACE) < 0)
    return COMP_ERR;

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

int compile_block(struct gab_eg *gab, bc *bc) {
  push_ctxframe(gab, bc, gab_string(gab, "anonymous"));

  int narguments = compile_parameters(gab, bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc) < 0)
    return COMP_ERR;

  struct gab_obj_block_proto *p = pop_ctxframe(gab, bc);

  push_op(bc, OP_BLOCK);
  push_short(bc, add_constant(mod(bc), __gab_obj(p)));

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_message(struct gab_eg *gab, bc *bc, gab_value name) {
  if (compile_message_spec(gab, bc, name) < 0)
    return COMP_ERR;

  push_ctxframe(gab, bc, name);

  int narguments = compile_parameters(gab, bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc) < 0)
    return COMP_ERR;

  struct gab_obj_block_proto *p = pop_ctxframe(gab, bc);

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_MESSAGE);
  push_short(bc, add_constant(mod(bc), __gab_obj(p)));

  gab_value m = gab_message(gab, name);
  uint16_t func_constant = add_constant(mod(bc), m);
  push_short(bc, func_constant);

  push_slot(bc, 1);
  return COMP_OK;
}

int compile_expression(struct gab_eg *gab, bc *bc) {
  return compile_exp_prec(gab, bc, kIN);
}

int compile_tuple(struct gab_eg *gab, bc *bc, uint8_t want, bool *mv_out) {
  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  uint8_t have = 0;
  bool mv;

  int result;
  do {
    if (optional_newline(bc) < 0)
      return COMP_ERR;

    mv = false;
    result = compile_exp_prec(gab, bc, kASSIGNMENT);

    if (result < 0)
      return COMP_ERR;

    mv = result == VAR_EXP;
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
     * If we want all possible values, try to patch a mv.
     * If we are successful, remove one from have.
     * This is because have's meaning changes to mean the number of
     * values in ADDITION to the mv ending the tuple.
     */
    have -= gab_mod_try_patch_mv(mod(bc), VAR_EXP);
  } else {
    /*
     * Here we want a specific number of values. Try to patch the mv to want
     * however many values we need in order to match up have and want. Again, we
     * subtract an extra one because in the case where we do patch, have's
     * meaning is now the number of ADDITIONAL values we have.
     */
    if (gab_mod_try_patch_mv(mod(bc), want - have + 1)) {
      // If we were successful, we have all the values we want.
      // We don't add the one because as far as the stack is concerned,
      // we have just filled the slots we wanted.
      push_slot(bc, want - have);
      have = want;
    }

    /*
     * If we failed to patch and still don't have enough, push some nils.
     */
    while (have < want) {
      // While we have fewer expressions than we want, push nulls.
      push_op(bc, OP_PUSH_NIL);
      push_slot(bc, 1);
      have++;
    }
  }

  if (mv_out)
    *mv_out = mv;

  assert(match_ctx(bc, kTUPLE));
  pop_ctx(bc, kTUPLE);

  return have;
}

int add_message_constant(struct gab_eg *gab, struct gab_mod *mod,
                         gab_value name) {
  return add_constant(mod, gab_message(gab, name));
}

int add_string_constant(struct gab_eg *gab, struct gab_mod *mod, s_char str) {
  return add_constant(mod, gab_nstring(gab, str.len, str.data));
}

int compile_rec_tup_internal_item(struct gab_eg *gab, bc *bc, uint8_t index) {
  return compile_expression(gab, bc);
}

int compile_rec_tup_internals(struct gab_eg *gab, bc *bc, bool *mv_out) {
  uint8_t size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_token(bc, TOKEN_RBRACE)) {

    int result = compile_rec_tup_internal_item(gab, bc, size);

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

  bool mv = gab_mod_try_patch_mv(mod(bc), VAR_EXP);

  if (mv_out)
    *mv_out = mv;

  return size - mv;
}

bool new_local_needs_shift(v_lvalue *lvalues, uint8_t index) {
  for (int i = 0; i < index; i++) {
    if (lvalues->data[i].kind != kNEW_LOCAL &&
        lvalues->data[i].kind != kNEW_REST_LOCAL)
      return true;
  }

  for (int i = index; i < lvalues->len; i++) {
    if (lvalues->data[i].kind == kPROP || lvalues->data[i].kind == kINDEX)
      return true;
  }

  return false;
}

uint8_t rest_lvalues(v_lvalue *lvalues) {
  uint8_t rest = 0;

  for (int i = 0; i < lvalues->len; i++)
    rest += lvalues->data[i].kind == kNEW_REST_LOCAL ||
            lvalues->data[i].kind == kEXISTING_REST_LOCAL;

  return rest;
}

uint8_t new_lvalues(v_lvalue *lvalues) {
  uint8_t new = 0;
  for (int i = 0; i < lvalues->len; i++)
    new += lvalues->data[i].kind == kNEW_LOCAL ||
           lvalues->data[i].kind == kNEW_REST_LOCAL;
  return new;
}

uint8_t preceding_existing_lvalues(v_lvalue *lvalues, int index) {
  uint8_t existing = 0;
  // for (int i = 0; i < index; i++)
  //   existing += lvalues->data[i].kind == kEXISTING_LOCAL ||
  //               lvalues->data[i].kind == kEXISTING_REST_LOCAL;

  for (int i = 0; i < index; i++)
    existing += (lvalues->data[i].kind == kPROP);

  for (int i = 0; i < index; i++)
    existing += 2 * (lvalues->data[i].kind == kINDEX);

  return existing;
}

int compile_assignment(struct gab_eg *gab, bc *bc, lvalue target) {
  bool first = !match_ctx(bc, kASSIGNMENT_TARGET);

  if (first && push_ctx(bc, kASSIGNMENT_TARGET) < 0)
    return COMP_ERR;

  int ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

  if (ctx < 0)
    return COMP_ERR;

  v_lvalue *lvalues = &bc->contexts[ctx].as.assignment_target;

  v_lvalue_push(lvalues, target);

  if (!first)
    return COMP_OK;

  uint16_t targets = 0;

  while (match_and_eat_token(bc, TOKEN_COMMA)) {
    if (compile_exp_prec(gab, bc, kASSIGNMENT) < 0)
      return COMP_ERR;

    targets++;
  }

  if (targets > lvalues->len) {
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  uint8_t n_rest_values = rest_lvalues(lvalues);
  uint8_t n_new_values = new_lvalues(lvalues);

  if (n_rest_values > 1) {
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                   "Only one rest value allowed");
    return COMP_ERR;
  }

  uint8_t want = n_rest_values ? VAR_EXP : lvalues->len;

  if (expect_token(bc, TOKEN_EQUAL) < 0)
    return COMP_ERR;

  gab_token prop_tok = prev_tok(bc);
  uint64_t prop_line = prev_line(bc);
  s_char prop_src = prev_src(bc);

  bool mv = false;

  int have = compile_tuple(gab, bc, want, &mv);

  // In the scenario where we want ALL possible values,
  // compile_tuple will only push one slot. Here we know
  // A minimum number of slots that we will have bc we call pack
  if (n_rest_values && have < n_new_values)
    push_slot(bc, n_new_values - have);

  if (have < 0)
    return COMP_ERR;

  if (!mv && n_rest_values) {
    push_op(bc, OP_VAR);
    push_byte(bc, have);
  }

  if (n_rest_values) {
    for (uint8_t i = 0; i < lvalues->len; i++) {
      if (lvalues->data[i].kind == kEXISTING_REST_LOCAL ||
          lvalues->data[i].kind == kNEW_REST_LOCAL) {
        uint8_t before = i;
        uint8_t after = lvalues->len - i - 1;

        gab_mod_push_pack(mod(bc), before, after, prop_tok, prop_line,
                          prop_src);

        int slots = lvalues->len - have;

        for (uint8_t j = i; j < lvalues->len; j++)
          v_lvalue_ref_at(lvalues, j)->slot += slots;

        if (slots > 0)
          push_slot(bc, slots - 1);
        else
          pop_slot(bc, -slots + 1);
      }
    }
  }

  for (uint8_t i = 0; i < lvalues->len; i++) {
    int lval_index = lvalues->len - 1 - i;
    lvalue lval = v_lvalue_val_at(lvalues, lval_index);

    switch (lval.kind) {
    case kNEW_REST_LOCAL:
    case kNEW_LOCAL:
      if (new_local_needs_shift(lvalues, lval_index)) {
        uint8_t shift_under = preceding_existing_lvalues(lvalues, lval_index);
        push_shift(bc, peek_slot(bc) - lval.slot + shift_under);

        // We've shifted a value underneath all the remaining lvalues.
        for (uint8_t j = 0; j < i; j++)
          v_lvalue_ref_at(lvalues, j)->slot++;
      }

      initialize_local(bc, lval.as.local);

      break;

    case kEXISTING_REST_LOCAL:
    case kEXISTING_LOCAL:
      push_store_local(bc, lval.as.local);

      push_pop(bc, 1);
      pop_slot(bc, 1);

      break;

    case kINDEX:
    case kPROP:
      push_shift(bc, peek_slot(bc) - lval.slot);
      break;
    }
  }

  for (uint8_t i = 0; i < lvalues->len; i++) {
    lvalue lval = v_lvalue_val_at(lvalues, lvalues->len - 1 - i);
    bool is_last_assignment = i + 1 == lvalues->len;

    switch (lval.kind) {
    case kNEW_LOCAL:
    case kNEW_REST_LOCAL:
    case kEXISTING_LOCAL:
    case kEXISTING_REST_LOCAL:
      if (is_last_assignment)
        push_load_local(bc, lval.as.local), push_slot(bc, 1);

      break;

    case kPROP: {
      gab_mod_push_op(mod(bc), OP_STORE_PROPERTY_ANA, prop_tok, prop_line,
                      prop_src);

      gab_mod_push_short(mod(bc), lval.as.property, prop_tok, prop_line,
                         prop_src);

      gab_mod_push_inline_cache(mod(bc), prop_tok, prop_line, prop_src);

      if (!is_last_assignment)
        push_pop(bc, 1);

      pop_slot(bc, 1 + !is_last_assignment);
      break;
    }

    case kINDEX: {
      uint16_t m =
          add_message_constant(gab, mod(bc), gab_string(gab, mGAB_SET));
      gab_mod_push_send(mod(bc), 2, m, false, prop_tok, prop_line, prop_src);

      if (!is_last_assignment)
        push_pop(bc, 1);

      pop_slot(bc, 2 + !is_last_assignment);
      break;
    }
    }
  }

  v_lvalue_destroy(lvalues);

  assert(match_ctx(bc, kASSIGNMENT_TARGET));
  pop_ctx(bc, kASSIGNMENT_TARGET);

  return COMP_OK;
}

// Forward decl
int compile_definition(struct gab_eg *gab, bc *bc, s_char name, s_char help);

int compile_rec_internal_item(struct gab_eg *gab, bc *bc) {
  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {

    gab_value val_name = prev_id(gab, bc);

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
      uint8_t value_in;
      int result = resolve_id(gab, bc, val_name, &value_in);

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

int compile_rec_internals(struct gab_eg *gab, bc *bc) {
  uint8_t size = 0;

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

int compile_record(struct gab_eg *gab, bc *bc) {
  int size = compile_rec_internals(gab, bc);

  if (size < 0)
    return COMP_ERR;

  push_op(bc, OP_RECORD);

  push_byte(bc, size);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_record_tuple(struct gab_eg *gab, bc *bc) {
  bool mv;
  int size = compile_rec_tup_internals(gab, bc, &mv);

  if (size < 0)
    return COMP_ERR;

  gab_mod_push_tuple(mod(bc), size, mv, prev_tok(bc), prev_line(bc),
                     prev_src(bc));

  pop_slot(bc, size);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_definition(struct gab_eg *gab, bc *bc, s_char name, s_char help) {
  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    gab_value val_name = gab_nstring(gab, name.len, name.data);

    uint8_t local = add_local(gab, bc, val_name, 0);

    if (compile_record(gab, bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_TYPE);

    push_op(bc, OP_DUP);

    push_slot(bc, 1);

    initialize_local(bc, local);

    return COMP_OK;
  }

  if (match_and_eat_token(bc, TOKEN_QUESTION))
    name.len++;

  else if (match_and_eat_token(bc, TOKEN_BANG))
    name.len++;

  gab_value val_name = gab_nstring(gab, name.len, name.data);

  // Create a local to store the new function in
  int local = add_local(gab, bc, val_name, 0);

  if (local < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_EQUAL)) {
    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    goto fin;
  }

  if (match_and_eat_token(bc, TOKEN_COMMA)) {
    size_t n = 1;
    do {
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      int l = add_local(gab, bc, prev_id(gab, bc), 0);

      if (l < 0)
        return COMP_ERR;

      n++;
    } while (match_and_eat_token(bc, TOKEN_COMMA));

    if (expect_token(bc, TOKEN_EQUAL) < 0)
      return COMP_ERR;

    if (compile_tuple(gab, bc, n, NULL) < 0)
      return COMP_ERR;

    // Initialize all the additional locals
    for (int i = 1; i < n; i++)
      initialize_local(bc, local + i);

    goto fin;
  }

  if (match_token(bc, TOKEN_LBRACE)) {
    if (compile_message(gab, bc, val_name) < 0)
      return COMP_ERR;

    goto fin;
  }

  if (compile_block(gab, bc) < 0)
    return COMP_ERR;

fin:
  initialize_local(bc, local);

  push_op(bc, OP_DUP);

  push_slot(bc, 1);

  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

int compile_exp_blk(struct gab_eg *gab, bc *bc, bool assignable) {
  return compile_block(gab, bc);
}

int compile_exp_then(struct gab_eg *gab, bc *bc, bool assignable) {
  uint64_t then_jump = gab_mod_push_jump(
      mod(bc), OP_JUMP_IF_FALSE, prev_tok(bc), prev_line(bc), prev_src(bc));

  push_scope(bc);

  int phantom = add_local(gab, bc, gab_string(gab, ""), 0);

  if (phantom < 0)
    return COMP_ERR;

  initialize_local(bc, phantom);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  pop_slot(bc, 1);

  push_load_local(bc, phantom);

  push_slot(bc, 1);

  pop_scope(bc);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), then_jump);

  return COMP_OK;
}

int compile_exp_else(struct gab_eg *gab, bc *bc, bool assignable) {
  uint64_t then_jump = gab_mod_push_jump(mod(bc), OP_JUMP_IF_TRUE, prev_tok(bc),
                                         prev_line(bc), prev_src(bc));
  push_scope(bc);

  int phantom = add_local(gab, bc, gab_string(gab, ""), 0);

  if (phantom < 0)
    return COMP_ERR;

  initialize_local(bc, phantom);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  pop_slot(bc, 1);

  push_load_local(bc, phantom);

  push_slot(bc, 1);

  pop_scope(bc);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), then_jump);

  return COMP_OK;
}

int compile_exp_mch(struct gab_eg *gab, bc *bc, bool assignable) {
  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  uint64_t next = 0;
  v_uint64_t done_jumps = {0};

  while (!match_and_eat_token(bc, TOKEN_ELSE)) {
    if (next != 0)
      gab_mod_patch_jump(mod(bc), next);

    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_MATCH);

    next = gab_mod_push_jump(mod(bc), OP_POP_JUMP_IF_FALSE, prev_tok(bc),
                             prev_line(bc), prev_src(bc));

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
    v_uint64_t_push(&done_jumps,
                    gab_mod_push_jump(mod(bc), OP_JUMP, prev_tok(bc),
                                      prev_line(bc), prev_src(bc)));
  }

  // If none of the cases match, the last jump should end up here.
  gab_mod_patch_jump(mod(bc), next);

  // Pop the pattern that we never matched
  push_pop(bc, 1);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  for (int i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    gab_mod_patch_jump(mod(bc), v_uint64_t_val_at(&done_jumps, i));
  }

  v_uint64_t_destroy(&done_jumps);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_bin(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);
  uint64_t line = prev_line(bc);
  s_char src = prev_src(bc);

  uint16_t m;

  switch (op) {
  case TOKEN_MINUS:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_SUB));
    break;

  case TOKEN_PLUS:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_ADD));
    break;

  case TOKEN_STAR:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_MUL));
    break;

  case TOKEN_SLASH:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_DIV));
    break;

  case TOKEN_PERCENT:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_MOD));
    break;

  case TOKEN_PIPE:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_BOR));
    break;

  case TOKEN_AMPERSAND:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_BND));
    break;

  case TOKEN_EQUAL_EQUAL:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_EQ));
    break;

  case TOKEN_LESSER:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_LT));
    break;

  case TOKEN_LESSER_EQUAL:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_LTE));
    break;

  case TOKEN_LESSER_LESSER:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_LSH));
    break;

  case TOKEN_GREATER:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_GT));
    break;

  case TOKEN_GREATER_EQUAL:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_GTE));
    break;

  case TOKEN_GREATER_GREATER:
    m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_RSH));
    break;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling binary expression");
    return COMP_ERR;
  }

  int result = compile_exp_prec(gab, bc, get_rule(op).prec + 1);

  pop_slot(bc, 1);

  if (result < 0)
    return COMP_ERR;

  gab_mod_push_send(mod(bc), 1, m, false, op, line, src);
  return VAR_EXP;
}

int compile_exp_una(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);

  int result = compile_exp_prec(gab, bc, kUNARY);

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

int encode_codepoint(char *out, int utf) {
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
a_char *parse_raw_str(bc *bc, s_char raw_str) {
  // The parsed string will be at most as long as the raw string.
  // (\n -> one char)
  char buffer[raw_str.len];
  size_t buf_end = 0;

  // Skip the first and last character (")
  size_t start = 0;
  while (raw_str.data[start] != '\'' && raw_str.data[start] != '"' &&
         raw_str.data[start] != '}')
    start++;

  for (size_t i = start + 1; i < raw_str.len - 1; i++) {
    int8_t c = raw_str.data[i];

    if (c == '\\') {

      switch (raw_str.data[i + 1]) {

      case 'r':
        buffer[buf_end++] = '\r';
        break;
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

        uint8_t cpl = 0;
        char codepoint[8] = {0};

        while (raw_str.data[i] != ']') {

          if (cpl == 7)
            return NULL;

          codepoint[cpl++] = raw_str.data[i++];
        }

        i++;

        long cp = strtol(codepoint, NULL, 16);

        int result = encode_codepoint(buffer + buf_end, cp);

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

  return a_char_create(buffer, buf_end);
};

int compile_strlit(struct gab_eg *gab, bc *bc) {
  a_char *parsed = parse_raw_str(bc, prev_src(bc));

  if (parsed == NULL) {
    compiler_error(bc, GAB_MALFORMED_STRING, "");
    return COMP_ERR;
  }

  uint16_t k = add_string_constant(gab, mod(bc),
                                   s_char_create(parsed->data, parsed->len));

  a_char_destroy(parsed);

  push_op(bc, OP_CONSTANT);
  push_short(bc, k);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_string(struct gab_eg *gab, bc *bc) {
  if (prev_tok(bc) == TOKEN_STRING)
    return compile_strlit(gab, bc);

  if (prev_tok(bc) == TOKEN_INTERPOLATION_BEGIN) {
    int result = COMP_OK;
    uint8_t n = 0;
    do {
      if (compile_strlit(gab, bc) < 0)
        return COMP_ERR;

      n++;

      if (match_token(bc, TOKEN_INTERPOLATION_END)) {
        goto fin;
      }

      if (compile_expression(gab, bc) < 0)
        return COMP_ERR;
      n++;

    } while ((result = match_and_eat_token(bc, TOKEN_INTERPOLATION_MIDDLE)));

  fin:
    if (result < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_INTERPOLATION_END) < 0)
      return COMP_ERR;

    if (compile_strlit(gab, bc) < 0)
      return COMP_ERR;
    n++;

    // Concat the final string.
    push_op(bc, OP_INTERPOLATE);
    push_byte(bc, n);

    pop_slot(bc, n - 1);

    return COMP_OK;
  }

  return COMP_ERR;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
int compile_exp_str(struct gab_eg *gab, bc *bc, bool assignable) {
  return compile_string(gab, bc);
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
int compile_exp_itp(struct gab_eg *gab, bc *bc, bool assignable) {
  return compile_string(gab, bc);
}

int compile_exp_grp(struct gab_eg *gab, bc *bc, bool assignable) {
  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_exp_num(struct gab_eg *gab, bc *bc, bool assignable) {
  double num = strtod((char *)prev_src(bc).data, NULL);
  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), gab_number(num)));
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_bool(struct gab_eg *gab, bc *bc, bool assignable) {
  push_op(bc, prev_tok(bc) == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_nil(struct gab_eg *gab, bc *bc, bool assignable) {
  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_def(struct gab_eg *gab, bc *bc, bool assignable) {
  size_t help_cmt_line = prev_line(bc);
  // 1 indexed, so correct for that if not the first line
  if (help_cmt_line > 0)
    help_cmt_line--;

  s_char help = v_s_char_val_at(&bc->source->line_comments, help_cmt_line);

  eat_token(bc);

  s_char name = {0};

  switch (prev_tok(bc)) {
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
    name = prev_src(bc);
    break;
  case TOKEN_LPAREN:
    name = prev_src(bc);
    if (match_and_eat_token(bc, TOKEN_RPAREN)) {
      name.len++;
      break;
    }
    break;
  case TOKEN_LBRACE: {
    name = prev_src(bc);
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

int compile_exp_arr(struct gab_eg *gab, bc *bc, bool assignable) {
  return compile_record_tuple(gab, bc);
}

int compile_exp_rec(struct gab_eg *gab, bc *bc, bool assignable) {
  return compile_record(gab, bc);
}

int compile_exp_ipm(struct gab_eg *gab, bc *bc, bool assignable) {
  s_char offset = trim_prev_src(bc);

  long local = strtoul((char *)offset.data, NULL, 10);

  if (local > GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (local - 1 >= f->narguments) {
    // There will always be at least one more local than arguments -
    //   The self local!
    if ((f->nlocals - f->narguments) > 1) {
      compiler_error(bc, GAB_INVALID_IMPLICIT, "");
      return COMP_ERR;
    }

    uint8_t missing_locals = local - f->narguments;

    f->narguments += missing_locals;

    push_slot(bc, missing_locals);

    for (int i = 0; i < missing_locals; i++) {
      int pad_local = compile_local(gab, bc, gab_number(local - i), 0);

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

int compile_exp_spd(struct gab_eg *gab, bc *bc, bool assignable) {
  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  gab_value id = prev_id(gab, bc);

  uint8_t index;
  int result = resolve_id(gab, bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    switch (result) {
    case COMP_ID_NOT_FOUND:
      index = compile_local(gab, bc, id, fVAR_MUTABLE);

      push_slot(bc, 1);

      return compile_assignment(gab, bc,
                                (lvalue){
                                    .kind = kNEW_REST_LOCAL,
                                    .slot = peek_slot(bc),
                                    .as.local = index,
                                });
    case COMP_RESOLVED_TO_LOCAL: {
      int ctx = peek_ctx(bc, kFRAME, 0);
      frame *f = &bc->contexts[ctx].as.frame;

      if (!(f->local_flags[index] & fVAR_MUTABLE)) {
        compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                       "Cannot assign to immutable variable.");
        return COMP_ERR;
      }

      return compile_assignment(gab, bc,
                                (lvalue){
                                    .kind = kEXISTING_REST_LOCAL,
                                    .slot = peek_slot(bc),
                                    .as.local = index,
                                });
    }
    case COMP_RESOLVED_TO_UPVALUE:
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                     "Captured variables are not assignable.");
      return COMP_ERR;
    default:
      return COMP_ERR;
    }
  }

  compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expression is not assignable");
  return COMP_ERR;
}

int compile_exp_idn(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_value id = prev_id(gab, bc);

  uint8_t index;
  int result = resolve_id(gab, bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      switch (result) {
      case COMP_ID_NOT_FOUND:
        index = compile_local(gab, bc, id, fVAR_MUTABLE);

        return compile_assignment(gab, bc,
                                  (lvalue){
                                      .kind = kNEW_LOCAL,
                                      .slot = peek_slot(bc),
                                      .as.local = index,
                                  });

      case COMP_RESOLVED_TO_LOCAL: {
        int ctx = peek_ctx(bc, kFRAME, 0);
        frame *f = &bc->contexts[ctx].as.frame;

        if (!(f->local_flags[index] & fVAR_MUTABLE)) {
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
      default:
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

int compile_exp_idx(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token prop_tok = prev_tok(bc);
  uint64_t prop_line = prev_line(bc);
  s_char prop_src = prev_src(bc);

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

  uint16_t m = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_GET));
  gab_mod_push_send(mod(bc), 1, m, false, prop_tok, prop_line, prop_src);

  pop_slot(bc, 2);
  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_dot(struct gab_eg *gab, bc *bc, bool assignable) {
  s_char prop_name = trim_prev_src(bc);

  int prop = add_string_constant(gab, mod(bc), prop_name);

  if (prop < 0)
    return COMP_ERR;

  gab_token prop_tok = prev_tok(bc);
  uint64_t prop_line = prev_line(bc);
  s_char prop_src = prev_src(bc);

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

  gab_mod_push_op(mod(bc), OP_LOAD_PROPERTY_ANA, prop_tok, prop_line, prop_src);

  gab_mod_push_short(mod(bc), prop, prop_tok, prop_line, prop_src);

  gab_mod_push_inline_cache(mod(bc), prop_tok, prop_line, prop_src);

  pop_slot(bc, 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_arg_list(struct gab_eg *gab, bc *bc, bool *mv_out) {
  int nargs = 0;

  if (!match_token(bc, TOKEN_RPAREN)) {

    nargs = compile_tuple(gab, bc, VAR_EXP, mv_out);

    if (nargs < 0)
      return COMP_ERR;
  }

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return nargs;
}

enum {
  fHAS_PAREN = 1 << 0,
  fHAS_BRACK = 1 << 1,
  fHAS_DO = 1 << 2,
  fHAS_STRING = 1 << 3,
};

int compile_arguments(struct gab_eg *gab, bc *bc, bool *mv_out, uint8_t flags) {
  int result = 0;
  *mv_out = false;

  if (flags & fHAS_PAREN ||
      (~flags & fHAS_DO && match_and_eat_token(bc, TOKEN_LPAREN))) {
    // Normal function args
    result = compile_arg_list(gab, bc, mv_out);

    if (result < 0)
      return COMP_ERR;
  }

  if (flags & fHAS_STRING ||
      match_and_eat_tokoneof(bc, TOKEN_STRING, TOKEN_INTERPOLATION_BEGIN)) {
    if (compile_string(gab, bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (flags & fHAS_BRACK || match_and_eat_token(bc, TOKEN_LBRACK)) {
    // record argument
    if (compile_record(gab, bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (flags & fHAS_DO || match_and_eat_token(bc, TOKEN_DO)) {
    // We are an anonyumous function
    if (compile_block(gab, bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  return result;
}

int compile_exp_emp(struct gab_eg *gab, bc *bc, bool assignable) {
  s_char message = prev_src(bc);
  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);

  s_char src = trim_prev_src(bc);

  gab_value val_name = gab_nstring(gab, src.len, src.data);

  uint16_t m = add_message_constant(gab, mod(bc), val_name);

  push_op(bc, OP_PUSH_NIL);

  push_slot(bc, 1);

  bool mv;
  int result = compile_arguments(gab, bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_mod_push_send(mod(bc), result, m, mv, tok, line, message);

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_amp(struct gab_eg *gab, bc *bc, bool assignable) {
  if (match_and_eat_token(bc, TOKEN_MESSAGE)) {
    gab_value val_name = prev_id(gab, bc);

    uint16_t f = add_message_constant(gab, mod(bc), val_name);

    push_op(bc, OP_CONSTANT);
    push_short(bc, f);

    push_slot(bc, 1);

    return COMP_OK;
  }

  const char *msg;

  eat_token(bc);

  switch (prev_tok(bc)) {
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

  uint16_t f = add_message_constant(gab, mod(bc), gab_string(gab, msg));

  push_op(bc, OP_CONSTANT);
  push_short(bc, f);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_snd(struct gab_eg *gab, bc *bc, bool assignable) {
  s_char src = prev_src(bc);
  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);

  s_char message = trim_prev_src(bc);

  gab_value val_name = gab_nstring(gab, message.len, message.data);

  uint16_t m = add_message_constant(gab, mod(bc), val_name);

  bool mv = false;
  int result = compile_arguments(gab, bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  gab_mod_push_send(mod(bc), result, m, mv, tok, line, src);

  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_cal(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_char call_src = prev_src(bc);

  bool mv = false;
  int result = compile_arguments(gab, bc, &mv, fHAS_PAREN);

  if (result < 0)
    return COMP_ERR;

  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  uint16_t msg = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, call_tok, call_line, call_src);

  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_bcal(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_char call_src = prev_src(bc);

  bool mv = false;
  int result = compile_arguments(gab, bc, &mv, fHAS_DO);

  if (result < 0)
    return COMP_ERR;
  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }
  pop_slot(bc, result);
  uint16_t msg = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_CALL));
  gab_mod_push_send(mod(bc), result, msg, mv, call_tok, call_line, call_src);
  return VAR_EXP;
}

int compile_exp_scal(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_char call_src = prev_src(bc);

  bool mv = false;
  int result = compile_arguments(gab, bc, &mv, 0);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, call_tok, call_line, call_src);

  return VAR_EXP;
}

int compile_exp_rcal(struct gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_char call_src = prev_src(bc);

  bool mv = false;
  int result = compile_arguments(gab, bc, &mv, fHAS_BRACK);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, call_tok, call_line, call_src);

  return VAR_EXP;
}

int compile_exp_and(struct gab_eg *gab, bc *bc, bool assignable) {
  uint64_t end_jump = gab_mod_push_jump(mod(bc), OP_LOGICAL_AND, prev_tok(bc),
                                        prev_line(bc), prev_src(bc));

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, kAND) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

int compile_exp_in(struct gab_eg *gab, bc *bc, bool assignable) {
  return optional_newline(bc);
}

int compile_exp_or(struct gab_eg *gab, bc *bc, bool assignable) {
  uint64_t end_jump = gab_mod_push_jump(mod(bc), OP_LOGICAL_OR, prev_tok(bc),
                                        prev_line(bc), prev_src(bc));

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, kOR) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

int compile_exp_prec(struct gab_eg *gab, bc *bc, prec_k prec) {
  if (eat_token(bc) < 0)
    return COMP_ERR;

  compile_rule rule = get_rule(prev_tok(bc));

  if (rule.prefix == NULL) {
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression.");
    return COMP_ERR;
  }

  bool assignable = prec <= kASSIGNMENT;

  int have = rule.prefix(gab, bc, assignable);

  if (have < 0)
    return COMP_ERR;

  while (prec <= get_rule(curr_tok(bc)).prec) {
    if (have < 0)
      return COMP_ERR;

    if (eat_token(bc) < 0)
      return COMP_ERR;

    rule = get_rule(prev_tok(bc));

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

int compile_exp_for(struct gab_eg *gab, bc *bc, bool assignable) {
  push_scope(bc);

  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);
  s_char src = prev_src(bc);

  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  uint8_t local_start = f->next_local;
  uint8_t nlooplocals = 0;
  int result;

  int mv = -1;

  do {
    switch (match_and_eat_token(bc, TOKEN_DOT_DOT)) {
    case COMP_OK:
      mv = nlooplocals;
      // Fallthrough
    case COMP_TOKEN_NO_MATCH: {
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      gab_value val_name = prev_id(gab, bc);

      int loc = compile_local(gab, bc, val_name, 0);

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

  initialize_local(bc, add_local(gab, bc, gab_string(gab, ""), 0));

  push_slot(bc, nlooplocals + 1);

  if (expect_token(bc, TOKEN_IN) < 0)
    return COMP_ERR;

  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  gab_mod_try_patch_mv(mod(bc), VAR_EXP);

  uint64_t loop = gab_mod_push_loop(mod(bc));

  if (mv >= 0)
    gab_mod_push_pack(mod(bc), mv, nlooplocals - mv, tok, line, src);

  uint64_t jump_start =
      gab_mod_push_iter(mod(bc), local_start, nlooplocals, tok, line, src);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_push_next(mod(bc), local_start + nlooplocals, tok, line, src);

  gab_mod_patch_loop(mod(bc), loop, tok, line, src);

  pop_scope(bc); /* Pop the scope once, after we exit the loop. */

  gab_mod_patch_jump(mod(bc), jump_start);

  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_lop(struct gab_eg *gab, bc *bc, bool assignable) {
  push_scope(bc);

  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);
  s_char src = prev_src(bc);

  uint64_t loop = gab_mod_push_loop(mod(bc));

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (match_and_eat_token(bc, TOKEN_UNTIL)) {
    if (compile_expression(gab, bc) < 0)
      return COMP_ERR;

    tok = prev_tok(bc);
    line = prev_line(bc);
    src = prev_src(bc);

    uint64_t jump =
        gab_mod_push_jump(mod(bc), OP_POP_JUMP_IF_TRUE, tok, line, src);

    gab_mod_patch_loop(mod(bc), loop, tok, line, src);

    gab_mod_patch_jump(mod(bc), jump);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    gab_mod_patch_loop(mod(bc), loop, tok, line, src);
  }

  push_op(bc, OP_PUSH_NIL);

  pop_scope(bc);
  return COMP_OK;
}

int compile_exp_sym(struct gab_eg *gab, bc *bc, bool assignable) {
  s_char name = trim_prev_src(bc);

  gab_value sym = gab_nstring(gab, name.len, name.data);

  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), __gab_obj(sym)));

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_yld(struct gab_eg *gab, bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    gab_value proto = gab_susproto(gab, mod(bc)->bytecode.len + 4, 1);

    uint16_t kproto = add_constant(mod(bc), proto);

    gab_mod_push_yield(mod(bc), kproto, 0, false, prev_tok(bc), prev_line(bc),
                       prev_src(bc));

    push_slot(bc, 1);

    return VAR_EXP;
  }

  bool mv;
  int result = compile_tuple(gab, bc, VAR_EXP, &mv);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_value proto = gab_susproto(gab, mod(bc)->bytecode.len + 4, 1);

  uint16_t kproto = add_constant(mod(bc), proto);

  push_slot(bc, 1);

  gab_mod_push_yield(mod(bc), kproto, result, mv, prev_tok(bc), prev_line(bc),
                     prev_src(bc));

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_rtn(struct gab_eg *gab, bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    push_slot(bc, 1);

    push_op(bc, OP_PUSH_NIL);

    gab_mod_push_return(mod(bc), 1, false, prev_tok(bc), prev_line(bc),
                        prev_src(bc));

    pop_slot(bc, 1);
    return COMP_OK;
  }

  bool mv;
  int result = compile_tuple(gab, bc, VAR_EXP, &mv);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_RET_MAX) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_mod_push_return(mod(bc), result, mv, prev_tok(bc), prev_line(bc),
                      prev_src(bc));

  pop_slot(bc, result);
  return COMP_OK;
}

// All of the expression compiling functions follow the naming convention
// compile_exp_XXX.
#define NONE()                                                                 \
  { NULL, NULL, kNONE, false }
#define PREC(prec)                                                             \
  { NULL, NULL, k##prec, false }
#define PREFIX(fnc)                                                            \
  { compile_exp_##fnc, NULL, kNONE, false }
#define INFIX(fnc, prec, is_multi)                                             \
  { NULL, compile_exp_##fnc, k##prec, is_multi }
#define PREFIX_INFIX(pre, in, prec, is_multi)                                  \
  { compile_exp_##pre, compile_exp_##in, k##prec, is_multi }

// ----------------Pratt Parsing Table ----------------------
const compile_rule rules[] = {
    INFIX(then, AND, false),                        // THEN
    INFIX(else, OR, false),                            // ELSE
    PREFIX_INFIX(blk, bcal, SEND, false),                       // DO
    PREFIX(for),                       // FOR
    INFIX(mch, MATCH, false),                       //MATCH 
    INFIX(in, IN, false),                            // IN
    NONE(),                            // END
    PREFIX(def),                       // DEF
    PREFIX(rtn),                       // RETURN
    PREFIX(yld),// YIELD
    PREFIX(lop),                       // LOOP
    NONE(),                       // UNTIL
    INFIX(bin, TERM, false),                  // PLUS
    PREFIX_INFIX(una, bin, TERM, false),      // MINUS
    INFIX(bin, FACTOR, false),                // STAR
    INFIX(bin, FACTOR, false),                // SLASH
    INFIX(bin, FACTOR, false),                // PERCENT
    NONE(),                            // COMMA
    NONE(),       // COLON
    PREFIX_INFIX(amp, bin, BITWISE_AND, false),           // AMPERSAND
    NONE(),           // DOLLAR
    PREFIX_INFIX(sym, dot, PROPERTY, true), // PROPERTY
    PREFIX_INFIX(emp, snd, SEND, true), // MESSAGE
    NONE(),              // DOT
    PREFIX(spd),                  // DOT_DOT
    NONE(),                            // EQUAL
    INFIX(bin, EQUALITY, false),              // EQUALEQUAL
    PREFIX(una),                            // QUESTION
    NONE(),                      // BANG
    NONE(),                            // AT
    NONE(),                            // COLON_EQUAL
    INFIX(bin, COMPARISON, false),            // LESSER
    INFIX(bin, EQUALITY, false),              // LESSEREQUAL
    INFIX(bin, TERM, false),              // LESSERLESSER
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
    PREFIX(idn),                       // ID
    PREFIX(ipm), //IMPLICIT
    PREFIX_INFIX(str, scal, SEND, false),                       // STRING
    PREFIX_INFIX(itp, scal, SEND, false),                       // INTERPOLATION END
    NONE(),                       // INTERPOLATION MIDDLE 
    NONE(),                       // INTERPOLATION END
    PREFIX(num),                       // NUMBER
    PREFIX(bool),                      // FALSE
    PREFIX(bool),                      // TRUE
    PREFIX(nil),                      // NIL
    NONE(),                      // NEWLINE
    NONE(),                            // EOF
    NONE(),                            // ERROR
};

compile_rule get_rule(gab_token k) { return rules[k]; }

gab_value compile(struct gab_eg *gab, bc *bc, gab_value name,
                  uint8_t narguments, gab_value arguments[narguments]) {
  struct gab_mod *new_mod = push_ctxframe(gab, bc, name);

  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;

  if (curr_tok(bc) == TOKEN_EOF)
    return gab_undefined;

  push_slot(bc, narguments);

  for (uint8_t i = 0; i < narguments; i++) {
    initialize_local(bc, add_local(gab, bc, arguments[i], 0));
  }

  if (compile_expressions_body(gab, bc) < 0)
    return gab_undefined;

  gab_mod_push_return(mod(bc), 1, false, prev_tok(bc), prev_line(bc),
                      prev_src(bc));

  struct gab_obj_block_proto *p = pop_ctxframe(gab, bc);

  gab_value proto = __gab_obj(p);

  add_constant(new_mod, proto);

  gab_value main = gab_block(gab, proto);

  add_constant(new_mod, __gab_obj(main));

  if (fGAB_DUMP_BYTECODE & bc->flags)
    gab_fdis(stdout, new_mod);

  return __gab_obj(main);
}

gab_value gab_bccompsend(struct gab_eg *gab, gab_value msg, gab_value receiver,
                         uint8_t flags, uint8_t narguments,
                         gab_value arguments[narguments]) {
  struct gab_mod *mod = gab_mod(gab, gab_string(gab, "send"), NULL);

  uint16_t message = add_constant(mod, msg);

  uint16_t constant = add_constant(mod, receiver);

  gab_mod_push_op(mod, OP_CONSTANT, 0, 0, (s_char){0});
  gab_mod_push_short(mod, constant, 0, 0, (s_char){0});

  for (uint8_t i = 0; i < narguments; i++) {
    uint16_t constant = add_constant(mod, arguments[i]);

    gab_mod_push_op(mod, OP_CONSTANT, 0, 0, (s_char){0});
    gab_mod_push_short(mod, constant, 0, 0, (s_char){0});
  }

  gab_mod_push_send(mod, narguments, message, false, 0, 0, (s_char){0});

  gab_mod_push_return(mod, 0, true, 0, 0, (s_char){0});

  uint8_t nlocals = narguments + 1;

  gab_value p =
      gab_blkproto(gab, mod, narguments, nlocals, nlocals, 0, NULL, NULL);

  add_constant(mod, p);

  gab_value main = gab_block(gab, p);

  return main;
}

gab_value gab_bccomp(struct gab_eg *gab, gab_value name, s_char source,
                     uint8_t flags, uint8_t narguments,
                     gab_value arguments[narguments]) {
  struct gab_src *src = gab_lex(gab, (char *)source.data, source.len);

  bc bc;
  gab_bc_create(&bc, src, flags);

  gab_value module = compile(gab, &bc, name, narguments, arguments);

  return module;
}

static void compiler_error(bc *bc, gab_status e, const char *help_fmt, ...) {
  if (bc->panic)
    return;

  bc->panic = true;

  if (bc->flags & fGAB_DUMP_ERROR) {
    struct gab_src *src = mod(bc)->source;

    va_list args;
    va_start(args, help_fmt);

    int ctx = peek_ctx(bc, kFRAME, 0);
    frame *f = &bc->contexts[ctx].as.frame;

    s_char curr_token = prev_src(bc);
    uint64_t line = prev_line(bc);

    if (line > 0)
      line--;

    s_char curr_src;

    if (line < src->source->len) {
      curr_src = v_s_char_val_at(&src->lines, line);
    } else {
      curr_src = s_char_cstr("");
    }

    const char *curr_src_start = curr_src.data;
    int curr_src_len = curr_src.len;

    while (is_whitespace(*curr_src_start)) {
      curr_src_start++;
      curr_src_len--;
      if (curr_src_len == 0)
        break;
    }

    a_char *curr_under = a_char_empty(curr_src_len);

    const char *tok_start, *tok_end;

    tok_start = curr_token.data;
    tok_end = curr_token.data + curr_token.len;

    const char *tok = gab_token_names[prev_tok(bc)];

    for (uint8_t i = 0; i < curr_under->len; i++) {
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
            f->mod->name, tok, curr_box, curr_color, line + 1,
            (int)curr_src_len, curr_src_start, (int)curr_under->len,
            curr_under->data);

    a_char_destroy(curr_under);

    fprintf(stderr,
            ANSI_COLOR_YELLOW "%s. \n\n" ANSI_COLOR_RESET "\t" ANSI_COLOR_GREEN,
            gab_status_names[e]);

    vfprintf(stderr, help_fmt, args);

    fprintf(stderr, "\n" ANSI_COLOR_RESET);

    va_end(args);
  }
}
