#include "include/compiler.h"
#include "include/char.h"
#include "include/colors.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/lexer.h"
#include "include/module.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct frame {
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
};

enum lvalue_k {
  kNEW_LOCAL,
  kEXISTING_LOCAL,
  kNEW_REST_LOCAL,
  kEXISTING_REST_LOCAL,
  kPROP,
  kINDEX,
};

struct lvalue {
  enum lvalue_k kind;

  uint16_t slot;

  union {
    uint16_t local;
    uint16_t property;
  } as;
};

#define T struct lvalue
#define NAME lvalue
#include "include/vector.h"

#define T uint16_t
#include "include/vector.h"

enum context_k {
  kIMPL,
  kFRAME,
  kTUPLE,
  kASSIGNMENT_TARGET,
  kMATCH_TARGET,
  kLOOP,
};

struct context {
  enum context_k kind;

  union {
    v_lvalue assignment_target;

    struct frame frame;
  } as;
};

struct bc {
  struct gab_src *source;

  struct gab_triple gab;

  size_t offset;

  uint8_t flags;

  bool panic;

  uint8_t ncontext;
  struct context contexts[cGAB_FUNCTION_DEF_NESTING_MAX];
};

static inline struct gab_gc *gc(struct bc *bc) { return bc->gab.gc; }
static inline struct gab_eg *eg(struct bc *bc) { return bc->gab.eg; }
static inline struct gab_vm *vm(struct bc *bc) { return bc->gab.vm; }
static inline struct gab_triple gab(struct bc *bc) { return bc->gab; }

/*
  Precedence rules for the parsing of expressions.
*/
enum prec_k {
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
};

typedef int (*compile_f)(struct bc *, bool);
struct compile_rule {
  compile_f prefix;
  compile_f infix;
  enum prec_k prec;
  bool multi_line;
};

void bc_create(struct bc *self, struct gab_triple gab, struct gab_src *source,
               uint8_t flags) {
  memset(self, 0, sizeof(*self));

  self->flags = flags;
  self->source = source;
  self->gab = gab;
}

void bc_destroy(struct bc *self) {}

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

static void compiler_error(struct bc *bc, enum gab_status e,
                           const char *help_fmt, ...);

static bool match_token(struct bc *bc, gab_token tok);

static int eat_token(struct bc *bc);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline bool match_token(struct bc *bc, gab_token tok) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset) == tok;
}

static inline bool match_terminator(struct bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL) || match_token(bc, TOKEN_EOF);
}

static int eat_token(struct bc *bc) {
  bc->offset++;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    compiler_error(bc, GAB_MALFORMED_TOKEN, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

static inline int match_and_eat_token(struct bc *bc, gab_token tok) {
  if (!match_token(bc, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

static inline int expect_token(struct bc *bc, gab_token tok) {
  if (!match_token(bc, tok)) {
    eat_token(bc);
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "Expected " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET,
                   gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(bc);
}

static inline int match_tokoneof(struct bc *bc, gab_token toka,
                                 gab_token tokb) {
  return match_token(bc, toka) || match_token(bc, tokb);
}

static inline int match_and_eat_tokoneof(struct bc *bc, gab_token toka,
                                         gab_token tokb) {
  if (!match_tokoneof(bc, toka, tokb))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

size_t prev_line(struct bc *bc) {
  return v_uint64_t_val_at(&bc->source->token_lines, bc->offset - 1);
}

gab_token curr_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset);
}

gab_token prev_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->source->tokens, bc->offset - 1);
}

s_char prev_src(struct bc *bc) {
  return v_s_char_val_at(&bc->source->token_srcs, bc->offset - 1);
}

s_char trim_prev_src(struct bc *bc) {
  s_char message = prev_src(bc);
  // SKip the ':' at the beginning
  message.data++;
  message.len--;

  return message;
}

static inline int peek_ctx(struct bc *bc, enum context_k kind, uint8_t depth) {
  int idx = bc->ncontext - 1;

  while (idx >= 0) {
    if (bc->contexts[idx].kind == kind)
      if (depth-- == 0)
        return idx;

    idx--;
  }

  return COMP_CONTEXT_NOT_FOUND;
}

static inline struct gab_mod *mod(struct bc *bc) {
  int frame = peek_ctx(bc, kFRAME, 0);
  return bc->contexts[frame].as.frame.mod;
}

static inline uint16_t add_constant(struct bc *bc, gab_value value) {
  gab_gciref(gab(bc), value);
  return gab_mod_add_constant(mod(bc), value);
}

gab_value prev_id(struct bc *bc) {
  s_char s = prev_src(bc);
  gab_value sv = gab_nstring(eg(bc), s.len, s.data);
  add_constant(bc, sv);
  return sv;
}

gab_value trim_prev_id(struct bc *bc) {
  s_char s = trim_prev_src(bc);
  gab_value sv = gab_nstring(eg(bc), s.len, s.data);
  add_constant(bc, sv);
  return sv;
}

static inline void push_op(struct bc *bc, gab_opcode op) {
  gab_mod_push_op(mod(bc), op, bc->offset - 1);
}

static inline void push_byte(struct bc *bc, uint8_t data) {
  gab_mod_push_byte(mod(bc), data, bc->offset - 1);
}

static inline void push_shift(struct bc *bc, uint8_t n) {
  if (n <= 1)
    return;

  push_op(bc, OP_SHIFT);

  push_byte(bc, n);
}

static inline void push_pop(struct bc *bc, uint8_t n) {
  gab_mod_push_pop(mod(bc), n, bc->offset - 1);
}

static inline void push_store_local(struct bc *bc, uint8_t local) {
  gab_mod_push_store_local(mod(bc), local, bc->offset - 1);
}

static inline void push_load_local(struct bc *bc, uint8_t local) {
  gab_mod_push_load_local(mod(bc), local, bc->offset - 1);
}

static inline void push_load_upvalue(struct bc *bc, uint8_t upv) {
  gab_mod_push_load_upvalue(mod(bc), upv, bc->offset - 1);
}

static inline void push_short(struct bc *bc, uint16_t data) {
  gab_mod_push_short(mod(bc), data, bc->offset - 1);
}

static inline uint16_t peek_slot(struct bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  return f->next_slot;
}

#if cGAB_DEBUG_BC

#define push_slot(bc, n) _push_slot(bc, n, __PRETTY_FUNCTION__, __LINE__)

static inline void _push_slot(bc *bc, uint16_t n, const char *file, int line) {
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

  printf("[PUSH] +%d -> %d |%s:%d\n", n, f->next_slot, file, line);
}

#else

static inline void push_slot(struct bc *bc, uint16_t n) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot + n >= UINT16_MAX) {
    compiler_error(bc, GAB_PANIC,
                   "Too many slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot += n;

  if (f->next_slot > f->nslots)
    f->nslots = f->next_slot;
}

#endif

#if cGAB_DEBUG_BC

#define pop_slot(bc, n) _pop_slot(bc, n, __FUNCTION__, __LINE__)

static inline void _pop_slot(bc *bc, uint16_t n, const char *file, int line) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot - n < 0) {
    compiler_error(bc, GAB_PANIC,
                   "Too few slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot -= n;

  printf("[POP] -%d -> %d |%s:%d\n", n, f->next_slot, file, line);
}

#else

static inline void pop_slot(struct bc *bc, uint16_t n) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot - n < 0) {
    compiler_error(bc, GAB_PANIC,
                   "Too few slots. This is an internal compiler error.\n");
    assert(0);
  }

  f->next_slot -= n;
}

#endif

static void initialize_local(struct bc *bc, uint8_t local) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->local_depths[local] != -1)
    return;

  f->local_depths[local] = f->scope_depth;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static int add_local(struct bc *bc, gab_value name, uint8_t flags) {
  add_constant(bc, name);
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

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

  push_slot(bc, 1);

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * upvalue.
 */
static int add_upvalue(struct bc *bc, int depth, uint8_t index, uint8_t flags) {
  int ctx = peek_ctx(bc, kFRAME, depth);

  struct frame *f = &bc->contexts[ctx].as.frame;
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
static int resolve_local(struct bc *bc, gab_value name, uint8_t depth) {
  int ctx = peek_ctx(bc, kFRAME, depth);

  if (ctx < 0) {
    return COMP_LOCAL_NOT_FOUND;
  }

  struct frame *f = &bc->contexts[ctx].as.frame;

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
static int resolve_upvalue(struct bc *bc, gab_value name, uint8_t depth) {
  if (depth >= bc->ncontext) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  int local = resolve_local(bc, name, depth + 1);

  if (local >= 0) {
    int ctx = peek_ctx(bc, kFRAME, depth + 1);
    struct frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->local_flags[local] |= fVAR_CAPTURED;

    if (flags & fVAR_MUTABLE) {
      compiler_error(bc, GAB_CAPTURED_MUTABLE, "");
      return COMP_ERR;
    }

    return add_upvalue(bc, depth, local, flags | fVAR_LOCAL);
  }

  int upvalue = resolve_upvalue(bc, name, depth + 1);
  if (upvalue >= 0) {
    int ctx = peek_ctx(bc, kFRAME, depth + 1);
    struct frame *f = &bc->contexts[ctx].as.frame;

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
static int resolve_id(struct bc *bc, gab_value name, uint8_t *value_in) {
  int arg = resolve_local(bc, name, 0);

  if (arg == COMP_ERR)
    return arg;

  if (arg == COMP_LOCAL_NOT_FOUND) {

    arg = resolve_upvalue(bc, name, 0);
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

static void push_scope(struct bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;
  f->scope_depth++;
}

static void pop_scope(struct bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->scope_depth--;

  uint8_t slots = 0;
  while (f->next_local > 1) {
    uint8_t local = f->next_local - 1;

    if (f->local_depths[local] <= f->scope_depth)
      break;

    f->next_local--;
    slots++;
  }

  push_op(bc, OP_DROP);
  push_byte(bc, slots);
}

static inline bool match_ctx(struct bc *bc, enum context_k kind) {
  return bc->contexts[bc->ncontext - 1].kind == kind;
}

static inline int pop_ctx(struct bc *bc, enum context_k kind) {
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

static inline int push_ctx(struct bc *bc, enum context_k kind) {
  if (bc->ncontext == UINT8_MAX) {
    compiler_error(bc, GAB_PANIC, "Too many nested contexts");
    return COMP_ERR;
  }

  memset(bc->contexts + bc->ncontext, 0, sizeof(struct context));
  bc->contexts[bc->ncontext].kind = kind;

  return bc->ncontext++;
}

static struct gab_mod *push_ctxframe(struct bc *bc, gab_value name) {
  int ctx = push_ctx(bc, kFRAME);

  if (ctx < 0)
    return NULL;

  struct context *c = bc->contexts + ctx;

  struct frame *f = &c->as.frame;

  memset(f, 0, sizeof(struct frame));

  struct gab_mod *mod = gab_mod(eg(bc), name, bc->source);
  f->mod = mod;

  add_constant(bc, name);

  initialize_local(bc, add_local(bc, gab_string(eg(bc), "self"), 0));

  return mod;
}

static gab_value pop_ctxframe(struct bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  uint8_t nupvalues = f->nupvalues;
  uint8_t nslots = f->nslots;
  uint8_t nargs = f->narguments;
  uint8_t nlocals = f->nlocals;

  gab_value p = gab_blkproto(eg(bc), (struct gab_blkproto_argt){
                                         .mod = f->mod,
                                         .nslots = nslots,
                                         .nlocals = nlocals,
                                         .narguments = nargs,
                                         .nupvalues = nupvalues,
                                         .flags = f->upv_flags,
                                         .indexes = f->upv_indexes,
                                     });

  assert(match_ctx(bc, kFRAME));

  if (pop_ctx(bc, kFRAME) < 0)
    return NULL;

  return p;
}

struct compile_rule get_rule(gab_token k);
int compile_exp_prec(struct bc *bc, enum prec_k prec);
int compile_expression(struct bc *bc);
int compile_tuple(struct bc *bc, uint8_t want, bool *mv_out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

static int compile_local(struct bc *bc, gab_value name, uint8_t flags) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

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

  return add_local(bc, name, flags);
}

int compile_parameters(struct bc *bc) {
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

      gab_value val_name = gab_nstring(eg(bc), name.len, name.data);

      int local = compile_local(bc, val_name, 0);

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
    gab_mod_push_pack(mod(bc), mv, narguments - mv, bc->offset - 1);

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = mv >= 0 ? VAR_EXP : narguments;
  return f->narguments;
}

static inline int skip_newlines(struct bc *bc) {
  while (match_token(bc, TOKEN_NEWLINE)) {
    if (eat_token(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline int optional_newline(struct bc *bc) {
  match_and_eat_token(bc, TOKEN_NEWLINE);
  return COMP_OK;
}

int compile_expressions_body(struct bc *bc) {
  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  if (compile_expression(bc) < 0)
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

    if (compile_expression(bc) < 0)
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

int compile_expressions(struct bc *bc) {
  push_scope(bc);

  uint64_t line = prev_line(bc);

  if (compile_expressions_body(bc) < 0)
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

int compile_block_body(struct bc *bc) {
  int result = compile_expressions(bc);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_push_return(mod(bc), result, false, bc->offset - 1);

  return COMP_OK;
}

int compile_message_spec(struct bc *bc, gab_value name) {
  if (expect_token(bc, TOKEN_LBRACE) < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_RBRACE)) {
    push_slot(bc, 1);
    push_op(bc, OP_PUSH_UNDEFINED);
    return COMP_OK;
  }

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_block(struct bc *bc) {
  push_ctxframe(bc, gab_string(eg(bc), "anonymous"));

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(bc) < 0)
    return COMP_ERR;

  gab_value p = pop_ctxframe(bc);

  push_op(bc, OP_BLOCK);
  push_short(bc, add_constant(bc, p));

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_message(struct bc *bc, gab_value name) {
  if (compile_message_spec(bc, name) < 0)
    return COMP_ERR;

  push_ctxframe(bc, name);

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(bc) < 0)
    return COMP_ERR;

  gab_value p = pop_ctxframe(bc);

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_MESSAGE);
  push_short(bc, add_constant(bc, p));

  gab_value m = gab_message(eg(bc), name);
  uint16_t func_constant = add_constant(bc, m);
  push_short(bc, func_constant);

  push_slot(bc, 1);
  return COMP_OK;
}

int compile_expression(struct bc *bc) { return compile_exp_prec(bc, kIN); }

int compile_tuple(struct bc *bc, uint8_t want, bool *mv_out) {
  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  uint8_t have = 0;
  bool mv;

  int result;
  do {
    if (optional_newline(bc) < 0)
      return COMP_ERR;

    mv = false;
    result = compile_exp_prec(bc, kASSIGNMENT);

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

int add_message_constant(struct bc *bc, gab_value name) {
  return add_constant(bc, gab_message(eg(bc), name));
}

int add_string_constant(struct bc *bc, s_char str) {
  return add_constant(bc, gab_nstring(eg(bc), str.len, str.data));
}

int compile_rec_tup_internal_item(struct bc *bc, uint8_t index) {
  return compile_expression(bc);
}

int compile_rec_tup_internals(struct bc *bc, bool *mv_out) {
  uint8_t size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_token(bc, TOKEN_RBRACE)) {

    int result = compile_rec_tup_internal_item(bc, size);

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

int compile_assignment(struct bc *bc, struct lvalue target) {
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
    if (compile_exp_prec(bc, kASSIGNMENT) < 0)
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

  size_t t = bc->offset - 1;

  bool mv = false;

  int have = compile_tuple(bc, want, &mv);

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

        gab_mod_push_pack(mod(bc), before, after, t);

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
    struct lvalue lval = v_lvalue_val_at(lvalues, lval_index);

    switch (lval.kind) {
    case kNEW_REST_LOCAL:
    case kNEW_LOCAL:
      initialize_local(bc, lval.as.local);
      [[fallthrough]];
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
    struct lvalue lval = v_lvalue_val_at(lvalues, lvalues->len - 1 - i);
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
      gab_mod_push_op(mod(bc), OP_STORE_PROPERTY_ANA, t);

      gab_mod_push_short(mod(bc), lval.as.property, t);

      gab_mod_push_inline_cache(mod(bc), t);

      if (!is_last_assignment)
        push_pop(bc, 1);

      pop_slot(bc, 1 + !is_last_assignment);
      break;
    }

    case kINDEX: {
      uint16_t m = add_message_constant(bc, gab_string(eg(bc), mGAB_SET));
      gab_mod_push_send(mod(bc), 2, m, false, t);

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
int compile_definition(struct bc *bc, s_char name, s_char help);

int compile_rec_internal_item(struct bc *bc) {
  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {

    gab_value val_name = prev_id(bc);

    push_slot(bc, 1);

    push_op(bc, OP_CONSTANT);
    push_short(bc, add_constant(bc, val_name));

    switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

    case COMP_OK: {
      if (compile_expression(bc) < 0)
        return COMP_ERR;

      return COMP_OK;
    }

    case COMP_TOKEN_NO_MATCH: {
      uint8_t value_in;
      int result = resolve_id(bc, val_name, &value_in);

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

    if (compile_expression(bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (compile_expression(bc) < 0)
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

int compile_rec_internals(struct bc *bc) {
  uint8_t size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (match_and_eat_token(bc, TOKEN_RBRACK) == COMP_TOKEN_NO_MATCH) {

    if (compile_rec_internal_item(bc) < 0)
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

int compile_record(struct bc *bc) {
  int size = compile_rec_internals(bc);

  if (size < 0)
    return COMP_ERR;

  push_op(bc, OP_RECORD);

  push_byte(bc, size);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_record_tuple(struct bc *bc) {
  bool mv;
  int size = compile_rec_tup_internals(bc, &mv);

  if (size < 0)
    return COMP_ERR;

  gab_mod_push_tuple(mod(bc), size, mv, bc->offset - 1);

  pop_slot(bc, size);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_definition(struct bc *bc, s_char name, s_char help) {
  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    gab_value val_name = gab_nstring(eg(bc), name.len, name.data);

    uint8_t local = add_local(bc, val_name, 0);

    if (compile_record(bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_TYPE);

    push_store_local(bc, local);

    initialize_local(bc, local);

    return COMP_OK;
  }

  if (match_and_eat_token(bc, TOKEN_QUESTION))
    name.len++;

  else if (match_and_eat_token(bc, TOKEN_BANG))
    name.len++;

  gab_value val_name = gab_nstring(eg(bc), name.len, name.data);

  // Create a local to store the new function in
  int local = add_local(bc, val_name, 0);

  if (local < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_EQUAL)) {
    if (compile_expression(bc) < 0)
      return COMP_ERR;

    goto fin;
  }

  if (match_and_eat_token(bc, TOKEN_COMMA)) {
    size_t n = 1;
    do {
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      int l = add_local(bc, prev_id(bc), 0);

      if (l < 0)
        return COMP_ERR;

      n++;
    } while (match_and_eat_token(bc, TOKEN_COMMA));

    if (expect_token(bc, TOKEN_EQUAL) < 0)
      return COMP_ERR;

    if (compile_tuple(bc, n, NULL) < 0)
      return COMP_ERR;

    // Initialize all the additional locals
    for (int i = n - 1; i > 0; i--) {
      push_store_local(bc, local + i);

      push_pop(bc, 1);

      initialize_local(bc, local + i);
    }

    goto fin;
  }

  if (match_token(bc, TOKEN_LBRACE)) {
    if (compile_message(bc, val_name) < 0)
      return COMP_ERR;

    goto fin;
  }

  if (compile_block(bc) < 0)
    return COMP_ERR;

fin:
  push_store_local(bc, local);

  initialize_local(bc, local);

  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

int compile_exp_blk(struct bc *bc, bool assignable) {
  return compile_block(bc);
}

int compile_exp_then(struct bc *bc, bool assignable) {
  uint64_t then_jump =
      gab_mod_push_jump(mod(bc), OP_JUMP_IF_FALSE, bc->offset - 1);

  push_scope(bc);

  int phantom = add_local(bc, gab_string(eg(bc), ""), 0);

  if (phantom < 0)
    return COMP_ERR;

  initialize_local(bc, phantom);

  if (compile_expressions(bc) < 0)
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

int compile_exp_else(struct bc *bc, bool assignable) {
  uint64_t then_jump =
      gab_mod_push_jump(mod(bc), OP_JUMP_IF_TRUE, bc->offset - 1);
  push_scope(bc);

  int phantom = add_local(bc, gab_string(eg(bc), ""), 0);

  if (phantom < 0)
    return COMP_ERR;

  initialize_local(bc, phantom);

  if (compile_expressions(bc) < 0)
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

int compile_exp_mch(struct bc *bc, bool assignable) {
  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  uint64_t next = 0;
  v_uint64_t done_jumps = {0};

  while (!match_and_eat_token(bc, TOKEN_ELSE)) {
    if (next != 0)
      gab_mod_patch_jump(mod(bc), next);

    if (compile_expression(bc) < 0)
      return COMP_ERR;

    push_op(bc, OP_MATCH);

    next = gab_mod_push_jump(mod(bc), OP_POP_JUMP_IF_FALSE, bc->offset - 1);

    pop_slot(bc, 1);

    if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
      return COMP_ERR;

    if (optional_newline(bc) < 0)
      return COMP_ERR;

    if (compile_expressions(bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    pop_slot(bc, 1);

    // Push a jump out of the match statement at the end of every case.
    v_uint64_t_push(&done_jumps,
                    gab_mod_push_jump(mod(bc), OP_JUMP, bc->offset - 1));
  }

  // If none of the cases match, the last jump should end up here.
  gab_mod_patch_jump(mod(bc), next);

  // Pop the pattern that we never matched
  push_pop(bc, 1);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expressions(bc) < 0)
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

int compile_exp_bin(struct bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);
  size_t t = bc->offset - 1;

  uint16_t m;

  switch (op) {
  case TOKEN_MINUS:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_SUB));
    break;

  case TOKEN_PLUS:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_ADD));
    break;

  case TOKEN_STAR:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_MUL));
    break;

  case TOKEN_SLASH:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_DIV));
    break;

  case TOKEN_PERCENT:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_MOD));
    break;

  case TOKEN_PIPE:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_BOR));
    break;

  case TOKEN_AMPERSAND:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_BND));
    break;

  case TOKEN_EQUAL_EQUAL:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_EQ));
    break;

  case TOKEN_LESSER:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_LT));
    break;

  case TOKEN_LESSER_EQUAL:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_LTE));
    break;

  case TOKEN_LESSER_LESSER:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_LSH));
    break;

  case TOKEN_GREATER:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_GT));
    break;

  case TOKEN_GREATER_EQUAL:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_GTE));
    break;

  case TOKEN_GREATER_GREATER:
    m = add_message_constant(bc, gab_string(eg(bc), mGAB_RSH));
    break;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling binary expression");
    return COMP_ERR;
  }

  int result = compile_exp_prec(bc, get_rule(op).prec + 1);

  pop_slot(bc, 1);

  if (result < 0)
    return COMP_ERR;

  gab_mod_push_send(mod(bc), 1, m, false, t);
  return VAR_EXP;
}

int compile_exp_una(struct bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);

  int result = compile_exp_prec(bc, kUNARY);

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
a_char *parse_raw_str(struct bc *bc, s_char raw_str) {
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

int compile_strlit(struct bc *bc) {
  a_char *parsed = parse_raw_str(bc, prev_src(bc));

  if (parsed == NULL) {
    compiler_error(bc, GAB_MALFORMED_STRING, "");
    return COMP_ERR;
  }

  uint16_t k =
      add_string_constant(bc, s_char_create(parsed->data, parsed->len));

  a_char_destroy(parsed);

  push_op(bc, OP_CONSTANT);
  push_short(bc, k);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_string(struct bc *bc) {
  if (prev_tok(bc) == TOKEN_STRING)
    return compile_strlit(bc);

  if (prev_tok(bc) == TOKEN_INTERPOLATION_BEGIN) {
    int result = COMP_OK;
    uint8_t n = 0;
    do {
      if (compile_strlit(bc) < 0)
        return COMP_ERR;

      n++;

      if (match_token(bc, TOKEN_INTERPOLATION_END)) {
        goto fin;
      }

      if (compile_expression(bc) < 0)
        return COMP_ERR;
      n++;

    } while ((result = match_and_eat_token(bc, TOKEN_INTERPOLATION_MIDDLE)));

  fin:
    if (result < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_INTERPOLATION_END) < 0)
      return COMP_ERR;

    if (compile_strlit(bc) < 0)
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
int compile_exp_str(struct bc *bc, bool assignable) {
  return compile_string(bc);
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
int compile_exp_itp(struct bc *bc, bool assignable) {
  return compile_string(bc);
}

int compile_exp_grp(struct bc *bc, bool assignable) {
  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_exp_num(struct bc *bc, bool assignable) {
  double num = strtod((char *)prev_src(bc).data, NULL);
  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(bc, gab_number(num)));
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_bool(struct bc *bc, bool assignable) {
  push_op(bc, prev_tok(bc) == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_nil(struct bc *bc, bool assignable) {
  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_def(struct bc *bc, bool assignable) {
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

  if (compile_definition(bc, name, help) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_exp_arr(struct bc *bc, bool assignable) {
  return compile_record_tuple(bc);
}

int compile_exp_rec(struct bc *bc, bool assignable) {
  return compile_record(bc);
}

int compile_exp_ipm(struct bc *bc, bool assignable) {
  s_char offset = trim_prev_src(bc);

  long local = strtoul((char *)offset.data, NULL, 10);

  if (local > GAB_LOCAL_MAX) {
    compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

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
      int pad_local = compile_local(bc, gab_number(local - i), 0);

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

int compile_exp_spd(struct bc *bc, bool assignable) {
  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  gab_value id = prev_id(bc);

  uint8_t index;
  int result = resolve_id(bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    switch (result) {
    case COMP_ID_NOT_FOUND:
      index = compile_local(bc, id, fVAR_MUTABLE);

      push_slot(bc, 1);

      return compile_assignment(bc, (struct lvalue){
                                        .kind = kNEW_REST_LOCAL,
                                        .slot = peek_slot(bc),
                                        .as.local = index,
                                    });
    case COMP_RESOLVED_TO_LOCAL: {
      int ctx = peek_ctx(bc, kFRAME, 0);
      struct frame *f = &bc->contexts[ctx].as.frame;

      if (!(f->local_flags[index] & fVAR_MUTABLE)) {
        compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                       "Cannot assign to immutable variable.");
        return COMP_ERR;
      }

      return compile_assignment(bc, (struct lvalue){
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

int compile_exp_idn(struct bc *bc, bool assignable) {
  gab_value id = prev_id(bc);

  uint8_t index;
  int result = resolve_id(bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      switch (result) {
      case COMP_ID_NOT_FOUND:
        index = compile_local(bc, id, fVAR_MUTABLE);

        int ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

        if (ctx > 0) {
          v_lvalue *lvalues = &bc->contexts[ctx].as.assignment_target;

          // We're adding a new local, which means we retroactively need
          // to add one to each other assignment target slot
          for (size_t i = 0; i < lvalues->len; i++)
            lvalues->data[i].slot++;
        }

        return compile_assignment(bc, (struct lvalue){
                                          .kind = kNEW_LOCAL,
                                          .slot = peek_slot(bc),
                                          .as.local = index,
                                      });

      case COMP_RESOLVED_TO_LOCAL: {
        int ctx = peek_ctx(bc, kFRAME, 0);
        struct frame *f = &bc->contexts[ctx].as.frame;

        if (!(f->local_flags[index] & fVAR_MUTABLE)) {
          compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                         "Cannot assign to immutable variable.");
          return COMP_ERR;
        }

        return compile_assignment(bc, (struct lvalue){
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

int compile_exp_idx(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      return compile_assignment(bc, (struct lvalue){
                                        .kind = kINDEX,
                                        .slot = peek_slot(bc),
                                    });
    }
  }

  uint16_t m = add_message_constant(bc, gab_string(eg(bc), mGAB_GET));
  gab_mod_push_send(mod(bc), 1, m, false, t);

  pop_slot(bc, 2);
  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_dot(struct bc *bc, bool assignable) {
  s_char prop_name = trim_prev_src(bc);

  int prop = add_string_constant(bc, prop_name);

  if (prop < 0)
    return COMP_ERR;

  size_t t = bc->offset - 1;

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      return compile_assignment(bc, (struct lvalue){
                                        .kind = kPROP,
                                        .slot = peek_slot(bc),
                                        .as.property = prop,
                                    });
    }
  }

  gab_mod_push_op(mod(bc), OP_LOAD_PROPERTY_ANA, t);

  gab_mod_push_short(mod(bc), prop, t);

  gab_mod_push_inline_cache(mod(bc), t);

  pop_slot(bc, 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_arg_list(struct bc *bc, bool *mv_out) {
  int nargs = 0;

  if (!match_token(bc, TOKEN_RPAREN)) {

    nargs = compile_tuple(bc, VAR_EXP, mv_out);

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

int compile_arguments(struct bc *bc, bool *mv_out, uint8_t flags) {
  int result = 0;
  *mv_out = false;

  if (flags & fHAS_PAREN ||
      (~flags & fHAS_DO && match_and_eat_token(bc, TOKEN_LPAREN))) {
    // Normal function args
    result = compile_arg_list(bc, mv_out);

    if (result < 0)
      return COMP_ERR;
  }

  if (flags & fHAS_STRING ||
      match_and_eat_tokoneof(bc, TOKEN_STRING, TOKEN_INTERPOLATION_BEGIN)) {
    if (compile_string(bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (flags & fHAS_BRACK || match_and_eat_token(bc, TOKEN_LBRACK)) {
    // record argument
    if (compile_record(bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (flags & fHAS_DO || match_and_eat_token(bc, TOKEN_DO)) {
    // We are an anonyumous function
    if (compile_block(bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  return result;
}

int compile_exp_emp(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  gab_value val_name = trim_prev_id(bc);

  uint16_t m = add_message_constant(bc, val_name);

  push_op(bc, OP_PUSH_NIL);

  push_slot(bc, 1);

  bool mv;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_mod_push_send(mod(bc), result, m, mv, t);

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_amp(struct bc *bc, bool assignable) {
  if (match_and_eat_token(bc, TOKEN_MESSAGE)) {
    gab_value val_name = trim_prev_id(bc);

    uint16_t f = add_message_constant(bc, val_name);

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

  uint16_t f = add_message_constant(bc, gab_string(eg(bc), msg));

  push_op(bc, OP_CONSTANT);
  push_short(bc, f);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_snd(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  gab_value val_name = trim_prev_id(bc);

  uint16_t m = add_message_constant(bc, val_name);

  bool mv = false;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  gab_mod_push_send(mod(bc), result, m, mv, t);

  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_cal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_PAREN);

  if (result < 0)
    return COMP_ERR;

  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  uint16_t msg = add_message_constant(bc, gab_string(eg(bc), mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, t);

  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_bcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_DO);

  if (result < 0)
    return COMP_ERR;
  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }
  pop_slot(bc, result);
  uint16_t msg = add_message_constant(bc, gab_string(eg(bc), mGAB_CALL));
  gab_mod_push_send(mod(bc), result, msg, mv, t);
  return VAR_EXP;
}

int compile_exp_scal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, 0);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(bc, gab_string(eg(bc), mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, t);

  return VAR_EXP;
}

int compile_exp_rcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_BRACK);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(bc, gab_string(eg(bc), mGAB_CALL));

  gab_mod_push_send(mod(bc), result, msg, mv, t);

  return VAR_EXP;
}

int compile_exp_and(struct bc *bc, bool assignable) {
  uint64_t end_jump =
      gab_mod_push_jump(mod(bc), OP_LOGICAL_AND, bc->offset - 1);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(bc, kAND) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

int compile_exp_in(struct bc *bc, bool assignable) {
  return optional_newline(bc);
}

int compile_exp_or(struct bc *bc, bool assignable) {
  uint64_t end_jump = gab_mod_push_jump(mod(bc), OP_LOGICAL_OR, bc->offset - 1);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(bc, kOR) < 0)
    return COMP_ERR;

  gab_mod_patch_jump(mod(bc), end_jump);

  return COMP_OK;
}

int compile_exp_prec(struct bc *bc, enum prec_k prec) {
  if (eat_token(bc) < 0)
    return COMP_ERR;

  struct compile_rule rule = get_rule(prev_tok(bc));

  if (rule.prefix == NULL) {
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression.");
    return COMP_ERR;
  }

  bool assignable = prec <= kASSIGNMENT;

  int have = rule.prefix(bc, assignable);

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
      have = rule.infix(bc, assignable);
    }
  }

  if (!assignable && match_token(bc, TOKEN_EQUAL)) {
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  return have;
}

int compile_exp_for(struct bc *bc, bool assignable) {
  push_scope(bc);

  size_t t = bc->offset - 1;

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

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

      gab_value val_name = prev_id(bc);

      int loc = compile_local(bc, val_name, 0);

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

  initialize_local(bc, add_local(bc, gab_string(eg(bc), ""), 0));

  if (expect_token(bc, TOKEN_IN) < 0)
    return COMP_ERR;

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  gab_mod_try_patch_mv(mod(bc), VAR_EXP);

  uint64_t loop = gab_mod_push_loop(mod(bc));

  if (mv >= 0)
    gab_mod_push_pack(mod(bc), mv, nlooplocals - mv, t);

  uint64_t jump_start = gab_mod_push_iter(mod(bc), local_start, nlooplocals, t);

  if (compile_expressions(bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_mod_push_next(mod(bc), local_start + nlooplocals, t);

  gab_mod_patch_loop(mod(bc), loop, t);

  pop_scope(bc); /* Pop the scope once, after we exit the loop. */

  gab_mod_patch_jump(mod(bc), jump_start);

  push_op(bc, OP_PUSH_NIL);
  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_lop(struct bc *bc, bool assignable) {
  push_scope(bc);

  size_t t = bc->offset - 1;

  uint64_t loop = gab_mod_push_loop(mod(bc));

  if (compile_expressions(bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1);

  if (match_and_eat_token(bc, TOKEN_UNTIL)) {
    if (compile_expression(bc) < 0)
      return COMP_ERR;

    size_t t = bc->offset - 1;

    uint64_t jump = gab_mod_push_jump(mod(bc), OP_POP_JUMP_IF_TRUE, t);

    gab_mod_patch_loop(mod(bc), loop, t);

    gab_mod_patch_jump(mod(bc), jump);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    gab_mod_patch_loop(mod(bc), loop, t);
  }

  push_op(bc, OP_PUSH_NIL);

  pop_scope(bc);
  return COMP_OK;
}

int compile_exp_sym(struct bc *bc, bool assignable) {
  gab_value sym = trim_prev_id(bc);

  push_op(bc, OP_CONSTANT);
  push_short(bc, add_constant(bc, sym));

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_yld(struct bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    gab_value proto = gab_susproto(eg(bc), mod(bc)->bytecode.len + 4, 1);

    uint16_t kproto = add_constant(bc, proto);

    gab_mod_push_yield(mod(bc), kproto, 0, false, bc->offset - 1);

    push_slot(bc, 1);

    return VAR_EXP;
  }

  bool mv;
  int result = compile_tuple(bc, VAR_EXP, &mv);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_value proto = gab_susproto(eg(bc), mod(bc)->bytecode.len + 4, 1);

  uint16_t kproto = add_constant(bc, proto);

  push_slot(bc, 1);

  gab_mod_push_yield(mod(bc), kproto, result, mv, bc->offset - 1);

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_rtn(struct bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    push_slot(bc, 1);

    push_op(bc, OP_PUSH_NIL);

    gab_mod_push_return(mod(bc), 1, false, bc->offset - 1);

    pop_slot(bc, 1);
    return COMP_OK;
  }

  bool mv;
  int result = compile_tuple(bc, VAR_EXP, &mv);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_RET_MAX) {
    compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_mod_push_return(mod(bc), result, mv, bc->offset - 1);

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
const struct compile_rule rules[] = {
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

struct compile_rule get_rule(gab_token k) { return rules[k]; }

gab_value compile(struct bc *bc, gab_value name, uint8_t narguments,
                  gab_value arguments[narguments]) {
  struct gab_mod *new_mod = push_ctxframe(bc, name);

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;

  if (curr_tok(bc) == TOKEN_EOF)
    return gab_undefined;

  for (uint8_t i = 0; i < narguments; i++) {
    initialize_local(bc, add_local(bc, arguments[i], 0));
  }

  if (compile_expressions_body(bc) < 0)
    return gab_undefined;

  gab_mod_push_return(mod(bc), 1, false, bc->offset - 1);

  gab_value p = pop_ctxframe(bc);

  gab_mod_add_constant(new_mod, p);
  gab_gciref(gab(bc), p);

  gab_value main = gab_block(eg(bc), p);
  gab_mod_add_constant(new_mod, main);
  gab_gciref(gab(bc), main);

  if (fGAB_DUMP_BYTECODE & bc->flags)
    gab_fdis(stdout, new_mod);

  return main;
}

gab_value gab_bccompsend(struct gab_triple gab, gab_value msg, gab_value receiver,
                         uint8_t flags, uint8_t narguments,
                         gab_value arguments[narguments]) {
  struct bc bc;
  bc_create(&bc, gab, NULL, flags);

  push_ctxframe(&bc, gab_string(gab.eg, "__send__"));

  uint16_t message = add_constant(&bc, msg);

  uint16_t constant = add_constant(&bc, receiver);

  gab_mod_push_op(mod(&bc), OP_CONSTANT, 0);
  gab_mod_push_short(mod(&bc), constant, 0);

  for (uint8_t i = 0; i < narguments; i++) {
    uint16_t constant = add_constant(&bc, arguments[i]);

    gab_mod_push_op(mod(&bc), OP_CONSTANT, 0);
    gab_mod_push_short(mod(&bc), constant, 0);
  }

  gab_mod_push_send(mod(&bc), narguments, message, false, 0);

  gab_mod_push_return(mod(&bc), 0, true, 0);

  uint8_t nlocals = narguments + 1;

  gab_value p = gab_blkproto(gab.eg, (struct gab_blkproto_argt){
                                      .mod = mod(&bc),
                                      .narguments = narguments,
                                      .nlocals = nlocals,
                                      .nslots = nlocals,
                                  });

  add_constant(&bc, p);

  gab_value main = gab_block(gab.eg, p);

  bc_destroy(&bc);

  return main;
}

gab_value gab_bccomp(struct gab_triple gab, gab_value name, s_char source,
                     uint8_t flags, uint8_t narguments,
                     gab_value arguments[narguments]) {
  struct gab_src *src = gab_lex(gab.eg, (char *)source.data, source.len);

  struct bc bc;
  bc_create(&bc, gab, src, flags);

  gab_value module = compile(&bc, name, narguments, arguments);

  bc_destroy(&bc);

  return module;
}

static void compiler_error(struct bc *bc, enum gab_status e,
                           const char *note_fmt, ...) {
  if (bc->panic)
    return;

  bc->panic = true;

  va_list va;
  va_start(va, note_fmt);

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  gab_verr(
      (struct gab_err_argt){
          .context = f->mod->name,
          .status = e,
          .tok = bc->offset - 1,
          .flags = bc->flags,
          .mod = mod(bc),
          .note_fmt = note_fmt,
      },
      va);

  va_end(va);
}
