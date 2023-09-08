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
  gab_mod *mod;

  int32_t scope_depth;

  uint8_t narguments;

  uint16_t next_slot;
  uint16_t nslots;

  uint8_t next_local;
  uint8_t nlocals;

  uint8_t nupvalues;

  uint8_t local_flags[GAB_LOCAL_MAX];
  int32_t local_depths[GAB_LOCAL_MAX];
  uint16_t local_names[GAB_LOCAL_MAX];

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

static const char *lvalue_k_names[] = {
    "NEW_LOCAL",           "EXISTING_LOCAL", "NEW_REST_LOCAL",
    "EXISTING_REST_LOCAL", "PROP",           "INDEX",
};

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
  gab_src *source;

  uint64_t offset;

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

typedef int32_t (*gab_compile_func)(gab_eg *, bc *, bool);
typedef struct gab_compile_rule compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  prec_k prec;
  bool multi_line;
};

void gab_bc_create(bc *self, gab_src *source, uint8_t flags) {
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

static int32_t eat_token(bc *bc);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline bool match_token(bc *bc, gab_token tok) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset) == tok;
}

static inline bool match_terminator(bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL) || match_token(bc, TOKEN_EOF);
}

static int32_t eat_token(bc *bc) {
  bc->offset++;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    compiler_error(bc, GAB_MALFORMED_TOKEN, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

static inline int32_t match_and_eat_token(bc *bc, gab_token tok) {
  if (!match_token(bc, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

static inline int32_t expect_token(bc *bc, gab_token tok) {
  if (!match_token(bc, tok)) {
    eat_token(bc);
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "Expected " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET,
                   gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(bc);
}

static inline int32_t match_tokoneof(bc *bc, gab_token toka, gab_token tokb) {
  return match_token(bc, toka) || match_token(bc, tokb);
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

s_int8_t prev_src(bc *bc) {
  return v_s_int8_t_val_at(&bc->source->tokens_src, bc->offset - 1);
}

s_int8_t trim_prev_src(bc *bc) {
  s_int8_t message = prev_src(bc);
  // SKip the ':' at the beginning
  message.data++;
  message.len--;

  return message;
}

gab_value prev_id(gab_eg *gab, bc *bc) {
  return __gab_obj(gab_obj_string_create(gab, prev_src(bc)));
}

static inline int32_t peek_ctx(bc *bc, context_k kind, uint8_t depth) {
  int32_t idx = bc->ncontext - 1;

  while (idx >= 0) {
    if (bc->contexts[idx].kind == kind)
      if (depth-- == 0)
        return idx;

    idx--;
  }

  return COMP_CONTEXT_NOT_FOUND;
}

static inline gab_mod *mod(bc *bc) {
  int32_t frame = peek_ctx(bc, kFRAME, 0);
  return bc->contexts[frame].as.frame.mod;
}

static inline uint16_t add_constant(gab_mod *mod, gab_value value) {
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
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  return f->next_slot;
}

static inline void push_slot(bc *bc, uint16_t n) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
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
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot - n < 0) {
    compiler_error(bc, GAB_PANIC,
                   "Too few slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot -= n;
}

static void initialize_local(bc *bc, uint8_t local) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->local_depths[local] != -1)
    return;

  f->local_depths[local] = f->scope_depth;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static int32_t add_local(gab_eg *gab, bc *bc, gab_value name, uint8_t flags) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->nlocals == GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  uint8_t local = f->next_local;
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
static int32_t add_upvalue(bc *bc, uint32_t depth, uint8_t index,
                           uint8_t flags) {
  int32_t ctx = peek_ctx(bc, kFRAME, depth);

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
static int32_t resolve_local(gab_eg *gab, bc *bc, gab_value name,
                             uint8_t depth) {
  int32_t ctx = peek_ctx(bc, kFRAME, depth);

  if (ctx < 0) {
    return COMP_LOCAL_NOT_FOUND;
  }

  frame *f = &bc->contexts[ctx].as.frame;

  for (int32_t local = f->next_local - 1; local >= 0; local--) {
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
static int32_t resolve_upvalue(gab_eg *gab, bc *bc, gab_value name,
                               uint8_t depth) {
  if (depth >= bc->ncontext) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  int32_t local = resolve_local(gab, bc, name, depth + 1);

  if (local >= 0) {
    int32_t ctx = peek_ctx(bc, kFRAME, depth + 1);
    frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->local_flags[local] |= fCAPTURED;

    if (flags & fMUTABLE) {
      compiler_error(bc, GAB_CAPTURED_MUTABLE, "");
      return COMP_ERR;
    }

    return add_upvalue(bc, depth, local, flags | fLOCAL);
  }

  int32_t upvalue = resolve_upvalue(gab, bc, name, depth + 1);
  if (upvalue >= 0) {
    int32_t ctx = peek_ctx(bc, kFRAME, depth + 1);
    frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->upv_flags[upvalue];
    return add_upvalue(bc, depth, upvalue, flags & ~fLOCAL);
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_RESOLVED_TO_LOCAL if the id was a local, and
 * COMP_RESOLVED_TO_UPVALUE if the id was an upvalue.
 */
static int32_t resolve_id(gab_eg *gab, bc *bc, gab_value name,
                          uint8_t *value_in) {
  int32_t arg = resolve_local(gab, bc, name, 0);

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

// static inline int32_t peek_scope(bc *bc) {
//   int32_t ctx = peek_ctx(bc, kFRAME, 0);
//   frame *f = &bc->contexts[ctx].as.frame;
//   return f->scope_depth;
// }

static void push_scope(bc *bc) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;
  f->scope_depth++;
}

static void pop_scope(bc *bc) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
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

static inline int32_t pop_ctx(bc *bc, context_k kind) {
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

static inline int32_t push_ctx(bc *bc, context_k kind) {
  if (bc->ncontext == UINT8_MAX) {
    compiler_error(bc, GAB_PANIC, "Too many nested contexts");
    return COMP_ERR;
  }

  memset(bc->contexts + bc->ncontext, 0, sizeof(context));
  bc->contexts[bc->ncontext].kind = kind;

  return bc->ncontext++;
}

static gab_mod *push_ctxframe(gab_eg *gab, bc *bc, gab_value name) {
  int32_t ctx = push_ctx(bc, kFRAME);

  if (ctx < 0)
    return NULL;

  context *c = bc->contexts + ctx;

  frame *f = &c->as.frame;

  memset(f, 0, sizeof(frame));

  gab_mod *mod = gab_mod_create(gab, name, bc->source);
  f->mod = mod;

  push_slot(bc, 1);

  initialize_local(bc, add_local(gab, bc, gab_string(gab, "self"), 0));

  return mod;
}

static gab_obj_block_proto *pop_ctxframe(gab_eg *gab, bc *bc) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  uint8_t nupvalues = f->nupvalues;
  uint8_t nslots = f->nslots;
  uint8_t nargs = f->narguments;
  uint8_t nlocals = f->nlocals;

  gab_obj_block_proto *p =
      gab_obj_prototype_create(gab, f->mod, nargs, nslots, nlocals, nupvalues,
                               f->upv_flags, f->upv_indexes);

  assert(match_ctx(bc, kFRAME));

  if (pop_ctx(bc, kFRAME) < 0)
    return NULL;

  return p;
}

compile_rule get_rule(gab_token k);
int32_t compile_exp_prec(gab_eg *gab, bc *bc, prec_k prec);
int32_t compile_expression(gab_eg *gab, bc *bc);
int32_t compile_tuple(gab_eg *gab, bc *bc, uint8_t want, bool *mv_out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

static int32_t compile_local(gab_eg *gab, bc *bc, gab_value name,
                             uint8_t flags) {
  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  for (int32_t local = f->next_local - 1; local >= 0; local--) {
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

int32_t compile_parameters(gab_eg *gab, bc *bc) {
  int32_t mv = -1;
  int32_t result = 0;
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

      s_int8_t name = prev_src(bc);

      gab_value val_name = __gab_obj(gab_obj_string_create(gab, name));

      int32_t local = compile_local(gab, bc, val_name, 0);

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

  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = mv >= 0 ? VAR_EXP : narguments;
  return f->narguments;
}

static inline int32_t skip_newlines(bc *bc) {
  while (match_token(bc, TOKEN_NEWLINE)) {
    if (eat_token(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline int32_t optional_newline(bc *bc) {
  match_and_eat_token(bc, TOKEN_NEWLINE);
  return COMP_OK;
}

int32_t compile_expressions_body(gab_eg *gab, bc *bc) {
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

int32_t compile_expressions(gab_eg *gab, bc *bc) {
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

int32_t compile_block_body(gab_eg *gab, bc *bc) {
  int32_t result = compile_expressions(gab, bc);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_push_return(mod(bc), result, false, prev_tok(bc), prev_line(bc),
                      prev_src(bc));

  return COMP_OK;
}

int32_t compile_message_spec(gab_eg *gab, bc *bc, gab_value name) {
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

  compiler_error(bc, GAB_MISSING_RECEIVER,
                 "Specify a receiver:\n\t\tdef %V[<receiver>] ... end", name);
  return COMP_ERR;
}

int32_t compile_block(gab_eg *gab, bc *bc) {
  push_ctxframe(gab, bc, gab_string(gab, "anonymous"));

  int32_t narguments = compile_parameters(gab, bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc) < 0)
    return COMP_ERR;

  gab_obj_block_proto *p = pop_ctxframe(gab, bc);

  push_op(bc, OP_BLOCK);
  push_short(bc, add_constant(mod(bc), __gab_obj(p)));

  push_slot(bc, 1);

  return COMP_OK;
}

int32_t compile_message(gab_eg *gab, bc *bc, gab_value name) {
  if (compile_message_spec(gab, bc, name) < 0)
    return COMP_ERR;

  push_ctxframe(gab, bc, name);

  int32_t narguments = compile_parameters(gab, bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc) < 0)
    return COMP_ERR;

  gab_obj_block_proto *p = pop_ctxframe(gab, bc);

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_MESSAGE);
  push_short(bc, add_constant(mod(bc), __gab_obj(p)));

  gab_obj_message *m = gab_obj_message_create(gab, name);
  uint16_t func_constant = add_constant(mod(bc), __gab_obj(m));
  push_short(bc, func_constant);

  push_slot(bc, 1);
  return COMP_OK;
}

int32_t compile_expression(gab_eg *gab, bc *bc) {
  return compile_exp_prec(gab, bc, kIN);
}

int32_t compile_tuple(gab_eg *gab, bc *bc, uint8_t want, bool *mv_out) {
  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  uint8_t have = 0;
  bool mv;

  int32_t result;
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

int32_t add_message_constant(gab_eg *gab, gab_mod *mod, gab_value name) {
  gab_obj_message *f = gab_obj_message_create(gab, name);
  return add_constant(mod, __gab_obj(f));
}

int32_t add_string_constant(gab_eg *gab, gab_mod *mod, s_int8_t str) {
  gab_obj_string *obj = gab_obj_string_create(gab, str);
  return add_constant(mod, __gab_obj(obj));
}

int32_t compile_rec_tup_internal_item(gab_eg *gab, bc *bc, uint8_t index) {
  return compile_expression(gab, bc);
}

int32_t compile_rec_tup_internals(gab_eg *gab, bc *bc, bool *mv_out) {
  uint8_t size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_token(bc, TOKEN_RBRACE)) {

    int32_t result = compile_rec_tup_internal_item(gab, bc, size);

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
  for (int32_t i = 0; i < index; i++) {
    if (lvalues->data[i].kind != kNEW_LOCAL &&
        lvalues->data[i].kind != kNEW_REST_LOCAL)
      return true;
  }

  for (int32_t i = index; i < lvalues->len; i++) {
    if (lvalues->data[i].kind == kPROP || lvalues->data[i].kind == kINDEX)
      return true;
  }

  return false;
}

bool rest_lvalues(v_lvalue *lvalues) {
  uint8_t rest = 0;

  for (int32_t i = 0; i < lvalues->len; i++)
    rest += lvalues->data[i].kind == kNEW_REST_LOCAL ||
            lvalues->data[i].kind == kEXISTING_REST_LOCAL;

  return rest;
}

int32_t compile_assignment(gab_eg *gab, bc *bc, lvalue target) {
  bool first = !match_ctx(bc, kASSIGNMENT_TARGET);

  if (first && push_ctx(bc, kASSIGNMENT_TARGET) < 0)
    return COMP_ERR;

  int32_t ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

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
  s_int8_t prop_src = prev_src(bc);

  bool mv = false;
  int32_t have = compile_tuple(gab, bc, want, &mv);

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

        int32_t slots = lvalues->len - have;

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
    lvalue lval = v_lvalue_val_at(lvalues, lvalues->len - 1 - i);

    switch (lval.kind) {
    case kNEW_REST_LOCAL:
    case kNEW_LOCAL:
      if (new_local_needs_shift(lvalues, lvalues->len - 1 - i)) {
        push_shift(bc, peek_slot(bc) - lval.as.local);

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
int32_t compile_definition(gab_eg *gab, bc *bc, s_int8_t name, s_int8_t help);

int32_t compile_rec_internal_item(gab_eg *gab, bc *bc) {
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
      int32_t result = resolve_id(gab, bc, val_name, &value_in);

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

int32_t compile_rec_internals(gab_eg *gab, bc *bc) {
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

int32_t compile_record(gab_eg *gab, bc *bc) {
  int32_t size = compile_rec_internals(gab, bc);

  if (size < 0)
    return COMP_ERR;

  push_op(bc, OP_RECORD);

  push_byte(bc, size);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return COMP_OK;
}

int32_t compile_record_tuple(gab_eg *gab, bc *bc) {
  bool mv;
  int32_t size = compile_rec_tup_internals(gab, bc, &mv);

  if (size < 0)
    return COMP_ERR;

  gab_mod_push_tuple(mod(bc), size, mv, prev_tok(bc), prev_line(bc),
                     prev_src(bc));

  pop_slot(bc, size);

  push_slot(bc, 1);

  return COMP_OK;
}

int32_t compile_definition(gab_eg *gab, bc *bc, s_int8_t name, s_int8_t help) {
  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    gab_value val_name = __gab_obj(gab_obj_string_create(gab, name));

    uint8_t local = add_local(gab, bc, val_name, 0);

    if (compile_record(gab, bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_TYPE);

    push_op(bc, OP_DUP);

    push_slot(bc, 1);

    initialize_local(bc, local);

    return COMP_OK;
  }

  // From now on, we know its a message definition.
  // Message names can end in a ? or a !
  // or, if the name is op
  if (match_and_eat_token(bc, TOKEN_QUESTION))
    name.len++;

  else if (match_and_eat_token(bc, TOKEN_BANG))
    name.len++;

  gab_value val_name = __gab_obj(gab_obj_string_create(gab, name));

  // Create a local to store the new function in
  uint8_t local = add_local(gab, bc, val_name, 0);

  // Now compile the impl
  if (compile_message(gab, bc, val_name) < 0)
    return COMP_ERR;

  initialize_local(bc, local);

  push_op(bc, OP_DUP);

  push_slot(bc, 1);

  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

int32_t compile_exp_blk(gab_eg *gab, bc *bc, bool assignable) {
  return compile_block(gab, bc);
}

int32_t compile_exp_then(gab_eg *gab, bc *bc, bool assignable) {
  uint64_t then_jump = gab_mod_push_jump(
      mod(bc), OP_JUMP_IF_FALSE, prev_tok(bc), prev_line(bc), prev_src(bc));

  pop_slot(bc, 1);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), then_jump);

  return COMP_OK;
}

int32_t compile_exp_else(gab_eg *gab, bc *bc, bool assignable) {
  uint64_t then_jump = gab_mod_push_jump(mod(bc), OP_JUMP_IF_TRUE, prev_tok(bc),
                                         prev_line(bc), prev_src(bc));

  pop_slot(bc, 1);

  if (compile_expressions(gab, bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), then_jump);

  return COMP_OK;
}

int32_t compile_exp_mch(gab_eg *gab, bc *bc, bool assignable) {
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

  for (int32_t i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    gab_mod_patch_jump(mod(bc), v_uint64_t_val_at(&done_jumps, i));
  }

  v_uint64_t_destroy(&done_jumps);

  push_slot(bc, 1);

  return COMP_OK;
}

int32_t compile_exp_bin(gab_eg *gab, bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);
  uint64_t line = prev_line(bc);
  s_int8_t src = prev_src(bc);

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

  int32_t result = compile_exp_prec(gab, bc, get_rule(op).prec + 1);

  pop_slot(bc, 1);

  if (result < 0)
    return COMP_ERR;

  gab_mod_push_send(mod(bc), 1, m, false, op, line, src);
  return VAR_EXP;
}

int32_t compile_exp_una(gab_eg *gab, bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);

  int32_t result = compile_exp_prec(gab, bc, kUNARY);

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

int32_t encode_codepoint(int8_t *out, uint32_t utf) {
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
a_int8_t *parse_raw_str(bc *bc, s_int8_t raw_str) {
  // The parsed string will be at most as long as the raw string.
  // (\n -> one char)
  int8_t buffer[raw_str.len];
  uint64_t buf_end = 0;

  // Skip the first and last character (")
  for (uint64_t i = 1; i < raw_str.len - 1; i++) {
    int8_t c = raw_str.data[i];

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

        uint8_t cpl = 0;
        char codepoint[8] = {0};

        while (raw_str.data[i] != ']') {

          if (cpl == 7)
            return NULL;

          codepoint[cpl++] = raw_str.data[i++];
        }

        i++;

        uint32_t cp = strtol(codepoint, NULL, 16);

        int32_t result = encode_codepoint(buffer + buf_end, cp);

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

  return a_int8_t_create(buffer, buf_end);
};

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
int32_t compile_exp_str(gab_eg *gab, bc *bc, bool assignable) {
  a_int8_t *parsed = parse_raw_str(bc, prev_src(bc));

  if (parsed == NULL) {
    compiler_error(bc, GAB_MALFORMED_STRING, "");
    return COMP_ERR;
  }

  uint16_t k = add_string_constant(gab, mod(bc),
                                   s_int8_t_create(parsed->data, parsed->len));

  a_int8_t_destroy(parsed);

  push_op(bc, OP_CONSTANT);
  push_short(bc, k);

  push_slot(bc, 1);

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
int32_t compile_exp_itp(gab_eg *gab, bc *bc, bool assignable) {
  int32_t result = COMP_OK;
  uint8_t n = 0;
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

int32_t compile_exp_grp(gab_eg *gab, bc *bc, bool assignable) {
  if (compile_expression(gab, bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int32_t compile_exp_num(gab_eg *gab, bc *bc, bool assignable) {
  double num = strtod((char *)prev_src(bc).data, NULL);
  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), gab_number(num)));
  push_slot(bc, 1);
  return COMP_OK;
}

int32_t compile_exp_bool(gab_eg *gab, bc *bc, bool assignable) {
  push_op(bc, prev_tok(bc) == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  push_slot(bc, 1);
  return COMP_OK;
}

int32_t compile_exp_nil(gab_eg *gab, bc *bc, bool assignable) {
  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);
  return COMP_OK;
}

int32_t compile_exp_def(gab_eg *gab, bc *bc, bool assignable) {
  size_t help_cmt_line = prev_line(bc);
  // 1 indexed, so correct for that if not the first line
  if (help_cmt_line > 0)
    help_cmt_line--;

  s_int8_t help = v_s_int8_t_val_at(&bc->source->line_comments, help_cmt_line);

  eat_token(bc);

  s_int8_t name = {0};

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

int32_t compile_exp_arr(gab_eg *gab, bc *bc, bool assignable) {
  return compile_record_tuple(gab, bc);
}

int32_t compile_exp_rec(gab_eg *gab, bc *bc, bool assignable) {
  return compile_record(gab, bc);
}

int32_t compile_exp_ipm(gab_eg *gab, bc *bc, bool assignable) {
  s_int8_t offset = trim_prev_src(bc);

  uint32_t local = strtoul((char *)offset.data, NULL, 10);

  if (local > GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  int32_t ctx = peek_ctx(bc, kFRAME, 0);
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

    for (int32_t i = 0; i < missing_locals; i++) {
      int32_t pad_local = compile_local(gab, bc, gab_number(local - i), 0);

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

int32_t compile_exp_spd(gab_eg *gab, bc *bc, bool assignable) {
  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  gab_value id = prev_id(gab, bc);

  uint8_t index;
  int32_t result = resolve_id(gab, bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    switch (result) {
    case COMP_ID_NOT_FOUND:
      index = compile_local(gab, bc, id, fMUTABLE);

      push_slot(bc, 1);

      return compile_assignment(gab, bc,
                                (lvalue){
                                    .kind = kNEW_REST_LOCAL,
                                    .slot = peek_slot(bc),
                                    .as.local = index,
                                });
    case COMP_RESOLVED_TO_LOCAL: {
      int32_t ctx = peek_ctx(bc, kFRAME, 0);
      frame *f = &bc->contexts[ctx].as.frame;

      if (!(f->local_flags[index] & fMUTABLE)) {
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

int32_t compile_exp_idn(gab_eg *gab, bc *bc, bool assignable) {
  gab_value id = prev_id(gab, bc);

  uint8_t index;
  int32_t result = resolve_id(gab, bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      switch (result) {
      case COMP_ID_NOT_FOUND:
        index = compile_local(gab, bc, id, fMUTABLE);

        return compile_assignment(gab, bc,
                                  (lvalue){
                                      .kind = kNEW_LOCAL,
                                      .slot = peek_slot(bc),
                                      .as.local = index,
                                  });

      case COMP_RESOLVED_TO_LOCAL: {
        int32_t ctx = peek_ctx(bc, kFRAME, 0);
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

int32_t compile_exp_idx(gab_eg *gab, bc *bc, bool assignable) {
  gab_token prop_tok = prev_tok(bc);
  uint64_t prop_line = prev_line(bc);
  s_int8_t prop_src = prev_src(bc);

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

int32_t compile_exp_dot(gab_eg *gab, bc *bc, bool assignable) {
  s_int8_t prop_name = trim_prev_src(bc);

  int32_t prop = add_string_constant(gab, mod(bc), prop_name);

  if (prop < 0)
    return COMP_ERR;

  gab_token prop_tok = prev_tok(bc);
  uint64_t prop_line = prev_line(bc);
  s_int8_t prop_src = prev_src(bc);

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

int32_t compile_arg_list(gab_eg *gab, bc *bc, bool *mv_out) {
  int32_t nargs = 0;

  if (!match_token(bc, TOKEN_RPAREN)) {

    nargs = compile_tuple(gab, bc, VAR_EXP, mv_out);

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

int32_t compile_arguments(gab_eg *gab, bc *bc, bool *mv_out, uint8_t flags) {
  int32_t result = 0;
  *mv_out = false;

  if (flags & fHAS_PAREN ||
      (~flags & fHAS_DO && match_and_eat_token(bc, TOKEN_LPAREN))) {
    // Normal function args
    result = compile_arg_list(gab, bc, mv_out);
    if (result < 0)
      return COMP_ERR;
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

int32_t compile_exp_emp(gab_eg *gab, bc *bc, bool assignable) {
  s_int8_t message = prev_src(bc);
  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);

  gab_value val_name = __gab_obj(gab_obj_string_create(gab, trim_prev_src(bc)));

  uint16_t m = add_message_constant(gab, mod(bc), val_name);

  push_op(bc, OP_PUSH_NIL);

  push_slot(bc, 1);

  bool mv;
  int32_t result = compile_arguments(gab, bc, &mv, 0);

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

int32_t compile_exp_amp(gab_eg *gab, bc *bc, bool assignable) {
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

int32_t compile_exp_snd(gab_eg *gab, bc *bc, bool assignable) {
  s_int8_t src = prev_src(bc);
  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);

  s_int8_t message = trim_prev_src(bc);

  gab_value val_name = __gab_obj(gab_obj_string_create(gab, message));

  uint16_t m = add_message_constant(gab, mod(bc), val_name);

  bool mv = false;
  int32_t result = compile_arguments(gab, bc, &mv, 0);

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

int32_t compile_exp_cal(gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_int8_t call_src = prev_src(bc);

  bool mv = false;
  int32_t result = compile_arguments(gab, bc, &mv, fHAS_PAREN);

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

int32_t compile_exp_bcal(gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_int8_t call_src = prev_src(bc);

  bool mv = false;
  int32_t result = compile_arguments(gab, bc, &mv, fHAS_DO);

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

int32_t compile_exp_rcal(gab_eg *gab, bc *bc, bool assignable) {
  gab_token call_tok = prev_tok(bc);
  uint64_t call_line = prev_line(bc);
  s_int8_t call_src = prev_src(bc);

  bool mv = false;
  int32_t result = compile_arguments(gab, bc, &mv, fHAS_BRACK);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(gab, mod(bc), gab_string(gab, mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, call_tok, call_line, call_src);

  return VAR_EXP;
}

int32_t compile_exp_and(gab_eg *gab, bc *bc, bool assignable) {
  uint64_t end_jump = gab_mod_push_jump(mod(bc), OP_LOGICAL_AND, prev_tok(bc),
                                        prev_line(bc), prev_src(bc));

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, kAND) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

int32_t compile_exp_in(gab_eg *gab, bc *bc, bool assignable) {
  return optional_newline(bc);
}

int32_t compile_exp_or(gab_eg *gab, bc *bc, bool assignable) {
  uint64_t end_jump = gab_mod_push_jump(mod(bc), OP_LOGICAL_OR, prev_tok(bc),
                                        prev_line(bc), prev_src(bc));

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, kOR) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

int32_t compile_exp_prec(gab_eg *gab, bc *bc, prec_k prec) {
  if (eat_token(bc) < 0)
    return COMP_ERR;

  compile_rule rule = get_rule(prev_tok(bc));

  if (rule.prefix == NULL) {
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression.");
    return COMP_ERR;
  }

  bool assignable = prec <= kASSIGNMENT;

  int32_t have = rule.prefix(gab, bc, assignable);

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

int32_t compile_exp_for(gab_eg *gab, bc *bc, bool assignable) {
  push_scope(bc);

  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);
  s_int8_t src = prev_src(bc);

  int32_t ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  uint8_t local_start = f->next_local;
  uint8_t nlooplocals = 0;
  int32_t result;

  int32_t mv = -1;

  do {
    switch (match_and_eat_token(bc, TOKEN_DOT_DOT)) {
    case COMP_OK:
      mv = nlooplocals;
      // Fallthrough
    case COMP_TOKEN_NO_MATCH: {
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      gab_value val_name = prev_id(gab, bc);

      int32_t loc = compile_local(gab, bc, val_name, 0);

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

int32_t compile_exp_lop(gab_eg *gab, bc *bc, bool assignable) {
  push_scope(bc);

  gab_token tok = prev_tok(bc);
  uint64_t line = prev_line(bc);
  s_int8_t src = prev_src(bc);

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

int32_t compile_exp_sym(gab_eg *gab, bc *bc, bool assignable) {
  s_int8_t name = trim_prev_src(bc);

  gab_value sym = __gab_obj(gab_obj_string_create(gab, name));

  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(mod(bc), __gab_obj(sym)));

  push_slot(bc, 1);

  return COMP_OK;
}

int32_t compile_exp_yld(gab_eg *gab, bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    gab_obj_suspense_proto *proto =
        gab_obj_suspense_proto_create(gab, mod(bc)->bytecode.len + 4, 1);

    uint16_t kproto = add_constant(mod(bc), __gab_obj(proto));

    gab_mod_push_yield(mod(bc), kproto, 0, false, prev_tok(bc), prev_line(bc),
                       prev_src(bc));

    push_slot(bc, 1);

    return VAR_EXP;
  }

  bool mv;
  int32_t result = compile_tuple(gab, bc, VAR_EXP, &mv);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_obj_suspense_proto *proto =
      gab_obj_suspense_proto_create(gab, mod(bc)->bytecode.len + 4, 1);

  uint16_t kproto = add_constant(mod(bc), __gab_obj(proto));

  push_slot(bc, 1);

  gab_mod_push_yield(mod(bc), kproto, result, mv, prev_tok(bc), prev_line(bc),
                     prev_src(bc));

  pop_slot(bc, result);

  return VAR_EXP;
}

int32_t compile_exp_rtn(gab_eg *gab, bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    push_slot(bc, 1);

    push_op(bc, OP_PUSH_NIL);

    gab_mod_push_return(mod(bc), 1, false, prev_tok(bc), prev_line(bc),
                        prev_src(bc));

    pop_slot(bc, 1);
    return COMP_OK;
  }

  bool mv;
  int32_t result = compile_tuple(gab, bc, VAR_EXP, &mv);

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
    PREFIX(str),                       // STRING
    PREFIX(itp),                       // INTERPOLATION
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

gab_value compile(gab_eg *gab, bc *bc, gab_value name, uint8_t narguments,
                  gab_value arguments[narguments]) {
  gab_mod *new_mod = push_ctxframe(gab, bc, name);

  int32_t ctx = peek_ctx(bc, kFRAME, 0);
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

  gab_obj_block_proto *p = pop_ctxframe(gab, bc);

  add_constant(new_mod, __gab_obj(p));

  gab_obj_block *main = gab_obj_block_create(gab, p);

  add_constant(new_mod, __gab_obj(main));

  if (fGAB_DUMP_BYTECODE & bc->flags)
    gab_fdis(stdout, new_mod);

  return __gab_obj(main);
}

gab_value gab_bccompsend(gab_eg *gab, gab_value msg, gab_value receiver,
                         uint8_t flags, uint8_t narguments,
                         gab_value arguments[narguments]) {
  gab_mod *mod = gab_mod_create(gab, gab_string(gab, "send"), NULL);

  uint16_t message = add_constant(mod, msg);

  uint16_t constant = add_constant(mod, receiver);

  gab_mod_push_op(mod, OP_CONSTANT, 0, 0, (s_int8_t){0});
  gab_mod_push_short(mod, constant, 0, 0, (s_int8_t){0});

  for (uint8_t i = 0; i < narguments; i++) {
    uint16_t constant = add_constant(mod, arguments[i]);

    gab_mod_push_op(mod, OP_CONSTANT, 0, 0, (s_int8_t){0});
    gab_mod_push_short(mod, constant, 0, 0, (s_int8_t){0});
  }

  gab_mod_push_send(mod, narguments, message, false, 0, 0, (s_int8_t){0});

  gab_mod_push_return(mod, 0, true, 0, 0, (s_int8_t){0});

  uint8_t nlocals = narguments + 1;

  gab_obj_block_proto *p = gab_obj_prototype_create(
      gab, mod, narguments, nlocals, nlocals, 0, NULL, NULL);

  add_constant(mod, __gab_obj(p));

  gab_obj_block *main = gab_obj_block_create(gab, p);

  return __gab_obj(main);
}

gab_value gab_bccomp(gab_eg *gab, gab_value name, s_int8_t source,
                     uint8_t flags, uint8_t narguments,
                     gab_value arguments[narguments]) {
  bc bc;
  gab_bc_create(&bc, gab_lex(gab, (char *)source.data, source.len), flags);

  gab_value module = compile(gab, &bc, name, narguments, arguments);

  return module;
}

static void compiler_error(bc *bc, gab_status e, const char *help_fmt, ...) {
  if (bc->panic)
    return;

  bc->panic = true;

  if (bc->flags & fGAB_DUMP_ERROR) {
    gab_src *src = mod(bc)->source;

    va_list args;
    va_start(args, help_fmt);

    int32_t ctx = peek_ctx(bc, kFRAME, 0);
    frame *f = &bc->contexts[ctx].as.frame;

    s_int8_t curr_token = prev_src(bc);
    uint64_t line = prev_line(bc);

    if (line > 0)
      line--;

    s_int8_t curr_src;

    if (line < src->source->len) {
      curr_src = v_s_int8_t_val_at(&src->lines, line);
    } else {
      curr_src = s_int8_t_cstr("");
    }

    const int8_t *curr_src_start = curr_src.data;
    int32_t curr_src_len = curr_src.len;

    while (is_whitespace(*curr_src_start)) {
      curr_src_start++;
      curr_src_len--;
      if (curr_src_len == 0)
        break;
    }

    a_int8_t *curr_under = a_int8_t_empty(curr_src_len);

    const int8_t *tok_start, *tok_end;

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
            (int32_t)curr_src_len, curr_src_start, (int32_t)curr_under->len,
            curr_under->data);

    a_int8_t_destroy(curr_under);

    fprintf(stderr,
            ANSI_COLOR_YELLOW "%s. \n\n" ANSI_COLOR_RESET "\t" ANSI_COLOR_GREEN,
            gab_status_names[e]);

    vfprintf(stderr, help_fmt, args);

    fprintf(stderr, "\n" ANSI_COLOR_RESET);

    va_end(args);
  }
}
