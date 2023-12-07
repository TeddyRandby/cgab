#define TOKEN_NAMES
#include "colors.h"
#include "core.h"
#include "engine.h"
#include "gab.h"
#include "lexer.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct frame {
  size_t offset;
  size_t jump;
  gab_value name;

  unsigned char prev_op;
  unsigned char prev_bb;
  unsigned char curr_bb;

  uint16_t next_slot;
  uint16_t nslots;

  int scope_depth;

  unsigned char narguments;
  unsigned char next_local;
  unsigned char nlocals;
  unsigned char nupvalues;

  char local_flags[GAB_LOCAL_MAX];
  int local_depths[GAB_LOCAL_MAX];
  gab_value local_names[GAB_LOCAL_MAX];

  char upv_flags[GAB_UPVALUE_MAX];
  char upv_indexes[GAB_UPVALUE_MAX];
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
    gab_value property;
  } as;
};

#define T struct lvalue
#define NAME lvalue
#include "vector.h"

#define T uint16_t
#include "vector.h"

enum context_k {
  kIMPL,
  kFRAME,
  kTUPLE,
  kASSIGNMENT_TARGET,
  kMATCH_TARGET,
  kLOOP,
  kCONTEXT_NKINDS,
};

struct context {
  enum context_k kind;

  union {
    v_lvalue assignment_target;
    v_uint64_t break_list;
    struct frame frame;
  } as;
};

struct bc {
  struct gab_src *src;

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
  self->src = source;
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
  return v_gab_token_val_at(&bc->src->tokens, bc->offset) == tok;
}

static inline bool match_terminator(struct bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL) || match_token(bc, TOKEN_EOF);
}

static int eat_token(struct bc *bc) {
  if (match_token(bc, TOKEN_EOF)) {
    compiler_error(bc, GAB_MALFORMED_TOKEN, "Unexpected EOF");
    return COMP_ERR;
  }

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
  return v_uint64_t_val_at(&bc->src->token_lines, bc->offset - 1);
}

gab_token curr_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset);
}

gab_token prev_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset - 1);
}

s_char prev_src(struct bc *bc) {
  return v_s_char_val_at(&bc->src->token_srcs, bc->offset - 1);
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

static inline void push_op(struct bc *bc, uint8_t op, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->prev_bb = f->curr_bb;
  f->prev_op = op;

  v_uint8_t_push(&bc->src->bytecode, op);
  v_uint64_t_push(&bc->src->bytecode_toks, t);
}

static inline void push_byte(struct bc *bc, uint8_t data, size_t t) {
  v_uint8_t_push(&bc->src->bytecode, data);
  v_uint64_t_push(&bc->src->bytecode_toks, t);
}

static inline void push_short(struct bc *bc, uint16_t data, size_t t) {
  push_byte(bc, (data >> 8) & 0xff, t);
  push_byte(bc, data & 0xff, t);
}

static inline void push_qword(struct bc *bc, uint64_t data, size_t t) {
  push_byte(bc, (data >> 56) & 0xff, t);
  push_byte(bc, (data >> 48) & 0xff, t);
  push_byte(bc, (data >> 40) & 0xff, t);
  push_byte(bc, (data >> 32) & 0xff, t);
  push_byte(bc, (data >> 24) & 0xff, t);
  push_byte(bc, (data >> 16) & 0xff, t);
  push_byte(bc, (data >> 8) & 0xff, t);
  push_byte(bc, data & 0xff, t);
}

static inline size_t addk(struct bc *bc, gab_value value) {
  gab_gciref(gab(bc), value);
  gab_egkeep(eg(bc), value);

  v_gab_value_push(&bc->src->constants, value);

  return bc->src->constants.len - 1;
}

static inline void push_loadk(struct bc *bc, gab_value k, size_t t) {
  push_op(bc, OP_CONSTANT, t);
  size_t c = addk(bc, k);
  assert(c < UINT16_MAX);
  push_short(bc, c, t);
}

static inline void push_shift(struct bc *bc, uint8_t n, size_t t) {
  if (n <= 1)
    return; // No op

  push_op(bc, OP_SHIFT, t);
  push_byte(bc, n, t);
}

static inline void push_loadl(struct bc *bc, uint8_t local, size_t t) {
  if (local > 8) {
    push_op(bc, OP_LOAD_LOCAL, t);
    push_byte(bc, local, t);
    return;
  }

  // We have a specialized opcode
  push_op(bc, OP_LOAD_LOCAL_0 + local, t);
}

static inline void push_loadu(struct bc *bc, uint8_t upv, size_t t) {
  if (upv > 8) {
    push_op(bc, OP_LOAD_UPVALUE, t);
    push_byte(bc, upv, t);
    return;
  }

  // We have a specialized opcode
  push_op(bc, OP_LOAD_UPVALUE_0 + upv, t);
}

static inline void push_storel(struct bc *bc, uint8_t local, size_t t) {
  if (local > 8) {
    push_op(bc, OP_STORE_LOCAL, t);
    push_byte(bc, local, t);
    return;
  }
  // We have a specialized opcode
  push_op(bc, OP_STORE_LOCAL_0 + local, t);
}

static inline uint8_t encode_arity(uint8_t have, bool mv) {
  return (have << 1) | mv;
}

static inline void push_ret(struct bc *bc, uint8_t have, bool mv, size_t t) {
  assert(have < 16);

  push_op(bc, OP_RETURN, t);
  push_byte(bc, encode_arity(have, mv), t);
}

static inline void push_yield(struct bc *bc, uint16_t p, uint8_t have, bool mv,
                              size_t t) {
  assert(have < 16);

  push_op(bc, OP_YIELD, t);
  push_short(bc, p, t);
  push_byte(bc, encode_arity(have, mv), t);
}

static inline void push_tuple(struct bc *bc, uint8_t have, bool mv, size_t t) {
  assert(have < 16);

  push_op(bc, OP_TUPLE, t);
  push_byte(bc, encode_arity(have, mv), t);
}

static inline void push_nnop(struct bc *bc, uint8_t n, size_t t) {
  for (int i = 0; i < n; i++)
    push_byte(bc, OP_NOP, t); // Don't count this as an op
}

static inline void push_dynsend(struct bc *bc, uint8_t have, bool mv,
                                size_t t) {
  assert(have < 16);

  push_op(bc, OP_DYNAMIC_SEND, t);
  push_nnop(bc, 2, t);
  push_byte(bc, encode_arity(have, mv), t);
  push_byte(bc, 1, t); // Default to wnating one

  push_nnop(bc, 24, t); // Push space for the inline cache (will be unused, but
                        // necessary to mimic sends)
}

static inline void push_send(struct bc *bc, uint16_t m, uint8_t have, bool mv,
                             size_t t) {
  assert(have < 16);

  push_op(bc, OP_SEND_ANA, t);
  push_short(bc, m, t);
  push_byte(bc, encode_arity(have, mv), t);
  push_byte(bc, 1, t); // Default to wnating one

  push_nnop(bc, 24, t); // Push space for the inline cache, and the version byte
}

static inline void push_pop(struct bc *bc, uint8_t n, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (n > 1) {
    push_op(bc, OP_POP_N, t);
    push_byte(bc, n, t);
    return;
  }

  if (f->curr_bb == f->prev_bb) {
    switch (f->prev_op) {
    case OP_PUSH_NIL:
    case OP_PUSH_UNDEFINED:
    case OP_PUSH_TRUE:
    case OP_PUSH_FALSE:
    case OP_LOAD_UPVALUE_0:
    case OP_LOAD_UPVALUE_1:
    case OP_LOAD_UPVALUE_2:
    case OP_LOAD_UPVALUE_3:
    case OP_LOAD_UPVALUE_4:
    case OP_LOAD_UPVALUE_5:
    case OP_LOAD_UPVALUE_6:
    case OP_LOAD_UPVALUE_7:
    case OP_LOAD_UPVALUE_8:
    case OP_LOAD_LOCAL_0:
    case OP_LOAD_LOCAL_1:
    case OP_LOAD_LOCAL_2:
    case OP_LOAD_LOCAL_3:
    case OP_LOAD_LOCAL_4:
    case OP_LOAD_LOCAL_5:
    case OP_LOAD_LOCAL_6:
    case OP_LOAD_LOCAL_7:
    case OP_LOAD_LOCAL_8:
      bc->src->bytecode.len -= 1;
      f->prev_op = bc->src->bytecode.data[bc->src->bytecode.len - 1];
      return;
    case OP_LOAD_UPVALUE:
    case OP_LOAD_LOCAL:
      bc->src->bytecode.len -= 2;
      f->prev_op = bc->src->bytecode.data[bc->src->bytecode.len - 1];
      return;
    case OP_CONSTANT:
      bc->src->bytecode.len -= 3;
      f->prev_op = bc->src->bytecode.data[bc->src->bytecode.len - 1];
      return;
    case OP_STORE_LOCAL_0:
    case OP_STORE_LOCAL_1:
    case OP_STORE_LOCAL_2:
    case OP_STORE_LOCAL_3:
    case OP_STORE_LOCAL_4:
    case OP_STORE_LOCAL_5:
    case OP_STORE_LOCAL_6:
    case OP_STORE_LOCAL_7:
    case OP_STORE_LOCAL_8:
      bc->src->bytecode.data[bc->src->bytecode.len - 1] += 9;
      f->prev_op = bc->src->bytecode.data[bc->src->bytecode.len - 1];
      return;

    default:
      break;
    }
  }

  push_op(bc, OP_POP, t);
}

static inline void push_next(struct bc *bc, uint8_t n, size_t t) {
  push_op(bc, OP_NEXT, t);
  push_byte(bc, n, t);
}

static inline void push_pack(struct bc *bc, uint8_t below, uint8_t above,
                             size_t t) {
  push_op(bc, OP_PACK, t);
  push_byte(bc, below, t);
  push_byte(bc, above, t);
}

static inline size_t push_iter(struct bc *bc, uint8_t want, uint8_t start,
                               size_t t) {
  push_op(bc, OP_ITER, t);
  push_byte(bc, want, t);
  push_byte(bc, start, t);
  push_nnop(bc, 2, t);

  return bc->src->bytecode.len - 2;
}

static inline size_t push_jump(struct bc *bc, uint8_t op, size_t t) {
  push_op(bc, op, t);
  push_nnop(bc, 2, t);

  return bc->src->bytecode.len - 2;
}

static inline void patch_jump(struct bc *bc, size_t jump) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->curr_bb++;

  size_t dist = bc->src->bytecode.len - jump - 2;

  assert(dist < UINT16_MAX);

  v_uint8_t_set(&bc->src->bytecode, jump, (dist >> 8) & 0xff);
  v_uint8_t_set(&bc->src->bytecode, jump + 1, dist & 0xff);
}

static inline size_t push_loop(struct bc *bc, size_t t) {
  return bc->src->bytecode.len;
}

static inline void patch_loop(struct bc *bc, size_t loop, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->curr_bb++;

  size_t dist = bc->src->bytecode.len - loop + 3;
  assert(dist < UINT16_MAX);
  push_op(bc, OP_LOOP, t);
  push_short(bc, dist, t);
}

static inline bool patch_mv(struct bc *bc, uint8_t want) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->prev_bb != f->curr_bb)
    return false;

  switch (f->prev_op) {
  case OP_SEND_ANA:
    v_uint8_t_set(&bc->src->bytecode, bc->src->bytecode.len - 25, want);
    return true;
  case OP_YIELD: {
    uint16_t offset =
        ((uint16_t)v_uint8_t_val_at(&bc->src->bytecode,
                                    bc->src->bytecode.len - 3)
         << 8) |
        v_uint8_t_val_at(&bc->src->bytecode, bc->src->bytecode.len - 2);

    gab_value value = v_gab_value_val_at(&bc->src->constants, offset);

    assert(gab_valkind(value) == kGAB_SPROTOTYPE);

    struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(value);

    proto->as.suspense.want = want;
    return true;
  }
  }

  return false;
}

gab_value prev_id(struct bc *bc) {
  s_char s = prev_src(bc);

  gab_value sv = gab_nstring(gab(bc), s.len, s.data);

  return sv;
}

gab_value trim_prev_id(struct bc *bc) {
  s_char s = trim_prev_src(bc);

  gab_value sv = gab_nstring(gab(bc), s.len, s.data);

  return sv;
}

static inline uint16_t peek_slot(struct bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  return f->next_slot;
}

#if cGAB_DEBUG_BC

#define push_slot(bc, n) _push_slot(bc, n, __PRETTY_FUNCTION__, __LINE__)

static inline void _push_slot(struct bc *bc, uint16_t n, const char *file,
                              int line) {
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

static inline void _pop_slot(struct bc *bc, uint16_t n, const char *file,
                             int line) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

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

static void init_local(struct bc *bc, uint8_t local) {
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
  addk(bc, name);
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

  while (f->next_local > 1) {
    uint8_t local = f->next_local - 1;

    if (f->local_depths[local] <= f->scope_depth)
      break;

    f->next_local--;
  }
}

static inline bool match_ctx(struct bc *bc, enum context_k kind) {
  return bc->contexts[bc->ncontext - 1].kind == kind;
}

static inline int pop_ctx(struct bc *bc, enum context_k kind) {
  while (bc->contexts[bc->ncontext - 1].kind != kind) {
    assert(bc->ncontext > 0 && "Internal compiler error: context stack "
                               "underflow");

    bc->ncontext--;
  }

  bc->ncontext--;

  return COMP_OK;
}

static inline int push_ctx(struct bc *bc, enum context_k kind) {
  assert(bc->ncontext < UINT8_MAX &&
         "Internal compiler error: context stack overflow");

  memset(bc->contexts + bc->ncontext, 0, sizeof(struct context));
  bc->contexts[bc->ncontext].kind = kind;

  return bc->ncontext++;
}

static void push_ctxframe(struct bc *bc, gab_value name) {
  int ctx = push_ctx(bc, kFRAME);

  assert(ctx >= 0 && "Failed to push frame context");

  struct context *c = bc->contexts + ctx;

  struct frame *f = &c->as.frame;

  memset(f, 0, sizeof(struct frame));

  f->jump = push_jump(bc, OP_JUMP, bc->offset - 1);
  f->offset = bc->src->bytecode.len;
  f->name = name;

  addk(bc, name);

  init_local(bc, add_local(bc, gab_string(gab(bc), "self"), 0));
}

static int pop_ctxloop(struct bc *bc) {
  int ctx = peek_ctx(bc, kLOOP, 0);

  v_uint64_t *breaks = &bc->contexts[ctx].as.break_list;

  for (int i = 0; i < breaks->len; i++) {
    uint64_t jump = breaks->data[i];
    patch_jump(bc, jump);
  }

  v_uint64_t_destroy(breaks);

  return pop_ctx(bc, kLOOP);
}

static gab_value pop_ctxframe(struct bc *bc) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  uint8_t nupvalues = f->nupvalues;
  uint8_t nslots = f->nslots;
  uint8_t nargs = f->narguments;
  uint8_t nlocals = f->nlocals;

  gab_value p = gab_bprototype(gab(bc), bc->src, f->name, f->offset,
                               (struct gab_blkproto_argt){
                                   .nslots = nslots,
                                   .nlocals = nlocals,
                                   .narguments = nargs,
                                   .nupvalues = nupvalues,
                                   .flags = f->upv_flags,
                                   .indexes = f->upv_indexes,
                               });

  gab_gciref(gab(bc), p);
  gab_egkeep(eg(bc), p);

  assert(match_ctx(bc, kFRAME));

  if (pop_ctx(bc, kFRAME) < 0)
    return gab_undefined;

  if (peek_ctx(bc, kFRAME, 0) != COMP_CONTEXT_NOT_FOUND)
    patch_jump(bc, f->jump);

  return p;
}

struct compile_rule get_rule(gab_token k);
int compile_exp_prec(struct bc *bc, enum prec_k prec);
int compile_expression(struct bc *bc);
int compile_tuple(struct bc *bc, uint8_t want, bool *mv_out);

bool curr_prefix(struct bc *bc) {
  return get_rule(curr_tok(bc)).prefix != NULL;
}

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

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

  push_loadk((bc), gab_nstring(gab(bc), parsed->len, parsed->data),
             bc->offset - 1);

  a_char_destroy(parsed);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_symbol(struct bc *bc) {
  gab_value sym = trim_prev_id(bc);

  push_loadk(bc, sym, bc->offset - 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_string(struct bc *bc) {
  if (prev_tok(bc) == TOKEN_SYMBOL)
    return compile_symbol(bc);

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
    push_op((bc), OP_INTERPOLATE, bc->offset - 1);
    push_byte((bc), n, bc->offset - 1);

    pop_slot(bc, n - 1);

    return COMP_OK;
  }

  return COMP_ERR;
}

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

    switch (match_and_eat_token(bc, TOKEN_STAR)) {
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

      gab_value val_name = gab_nstring(gab(bc), name.len, name.data);

      int local = compile_local(bc, val_name, 0);

      if (local < 0)
        return COMP_ERR;

      init_local(bc, local);

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
    push_pack(bc, mv, narguments - mv, bc->offset - 1);

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
    push_pop(bc, 1, bc->offset - 1);

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

  bool mv = patch_mv(bc, VAR_EXP);

  push_ret(bc, !mv, mv, bc->offset - 1);

  return COMP_OK;
}

int compile_message_spec(struct bc *bc) {
  if (expect_token(bc, TOKEN_LBRACE) < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_RBRACE)) {
    push_slot(bc, 1);
    push_op(bc, OP_PUSH_UNDEFINED, bc->offset - 1);
    return COMP_OK;
  }

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_block(struct bc *bc, gab_value name) {
  push_ctxframe(bc, name);

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(bc) < 0)
    return COMP_ERR;

  gab_value p = pop_ctxframe(bc);
  if (p == gab_undefined)
    return COMP_ERR;

  push_op(bc, OP_BLOCK, bc->offset - 1);
  push_short(bc, addk(bc, p), bc->offset - 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_message(struct bc *bc, gab_value name) {
  if (compile_message_spec(bc) < 0)
    return COMP_ERR;

  push_ctxframe(bc, name);

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(bc) < 0)
    return COMP_ERR;

  gab_value p = pop_ctxframe(bc);
  if (p == gab_undefined)
    return COMP_ERR;

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, OP_MESSAGE, bc->offset - 1);
  push_short(bc, addk(bc, p), bc->offset - 1);

  gab_value m = gab_message(gab(bc), name);
  push_short(bc, addk(bc, m), bc->offset - 1);

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

    if (!curr_prefix(bc))
      break;

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
    have -= patch_mv(bc, VAR_EXP);
  } else {
    /*
     * Here we want a specific number of values. Try to patch the mv to want
     * however many values we need in order to match up have and want. Again, we
     * subtract an extra one because in the case where we do patch, have's
     * meaning is now the number of ADDITIONAL values we have.
     */
    if (patch_mv(bc, want - have + 1)) {
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
      push_op(bc, OP_PUSH_NIL, bc->offset - 1);
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
  return addk(bc, gab_message(gab(bc), name));
}

int add_string_constant(struct bc *bc, s_char str) {
  return addk(bc, gab_nstring(gab(bc), str.len, str.data));
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

  bool mv = patch_mv(bc, VAR_EXP);

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
    push_op(bc, OP_VAR, bc->offset - 1);
    push_byte(bc, have, bc->offset - 1);
  }

  if (n_rest_values) {
    for (uint8_t i = 0; i < lvalues->len; i++) {
      if (lvalues->data[i].kind == kEXISTING_REST_LOCAL ||
          lvalues->data[i].kind == kNEW_REST_LOCAL) {
        uint8_t before = i;
        uint8_t after = lvalues->len - i - 1;

        push_pack(bc, before, after, t);

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
      init_local(bc, lval.as.local);
      [[fallthrough]];
    case kEXISTING_REST_LOCAL:
    case kEXISTING_LOCAL:
      push_storel(bc, lval.as.local, t);
      push_pop(bc, 1, t);
      pop_slot(bc, 1);
      break;

    case kINDEX:
    case kPROP:
      push_shift(bc, peek_slot(bc) - lval.slot, t);
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
        push_loadl(bc, lval.as.local, t), push_slot(bc, 1);

      break;

    case kPROP: {
      uint16_t m = add_message_constant(bc, lval.as.property);
      push_send(bc, m, 1, false, t);

      if (!is_last_assignment)
        push_pop(bc, 1, t);

      pop_slot(bc, 1 + !is_last_assignment);
      break;
    }

    case kINDEX: {
      uint16_t m = add_message_constant(bc, gab_string(gab(bc), mGAB_SET));
      push_send(bc, m, 2, false, t);

      if (!is_last_assignment)
        push_pop(bc, 1, t);

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

int compile_definition(struct bc *bc, s_char name);

int compile_rec_internal_item(struct bc *bc) {
  if (match_and_eat_token(bc, TOKEN_SYMBOL) ||
      match_and_eat_token(bc, TOKEN_STRING) ||
      match_and_eat_token(bc, TOKEN_INTERPOLATION_BEGIN)) {
    size_t t = bc->offset - 1;

    if (compile_string(bc) < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_COLON)) {
      if (compile_expression(bc) < 0)
        return COMP_ERR;
    } else {
      push_op((bc), OP_PUSH_TRUE, t);
      push_slot(bc, 1);
    }

    return COMP_OK;
  }

  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {
    gab_value val_name = prev_id(bc);

    size_t t = bc->offset - 1;

    push_slot(bc, 1);

    push_loadk(bc, val_name, t);

    switch (match_and_eat_token(bc, TOKEN_COLON)) {

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
        push_loadl((bc), value_in, t);
        return COMP_OK;

      case COMP_RESOLVED_TO_UPVALUE:
        push_loadu((bc), value_in, t);
        return COMP_OK;

      case COMP_ID_NOT_FOUND:
        push_op((bc), OP_PUSH_TRUE, t);
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
    size_t t = bc->offset - 1;

    if (compile_expression(bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_COLON)) {
      if (compile_expression(bc) < 0)
        return COMP_ERR;
    } else {
      push_op((bc), OP_PUSH_TRUE, t);
      push_slot(bc, 1);
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

  push_op((bc), OP_RECORD, bc->offset - 1);
  push_byte((bc), size, bc->offset - 1);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_record_tuple(struct bc *bc) {
  bool mv;
  int size = compile_rec_tup_internals(bc, &mv);

  if (size < 0)
    return COMP_ERR;

  push_tuple((bc), size, mv, bc->offset - 1);

  pop_slot(bc, size);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_definition(struct bc *bc, s_char name) {
  size_t t = bc->offset - 1;

  if (match_and_eat_token(bc, TOKEN_QUESTION))
    name.len++;

  else if (match_and_eat_token(bc, TOKEN_BANG))
    name.len++;

  gab_value val_name = gab_nstring(gab(bc), name.len, name.data);

  // Create a local to store the new function in
  int local = add_local(bc, val_name, 0);

  if (local < 0)
    return COMP_ERR;

  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    if (compile_record(bc) < 0)
      return COMP_ERR;

    push_op((bc), OP_TYPE, t);

    goto fin;
  }

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

    t = bc->offset - 1;

    if (compile_tuple(bc, n, NULL) < 0)
      return COMP_ERR;

    // Initialize all the additional locals
    for (int i = n - 1; i > 0; i--) {
      push_storel((bc), local + i, t);
      push_pop((bc), 1, t);
      init_local(bc, local + i);
    }

    goto fin;
  }

  if (match_token(bc, TOKEN_LBRACE)) {
    if (compile_message(bc, val_name) < 0)
      return COMP_ERR;

    goto fin;
  }

  if (compile_block(bc, val_name) < 0)
    return COMP_ERR;

fin:
  push_storel((bc), local, t);

  init_local(bc, local);

  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

int compile_exp_blk(struct bc *bc, bool assignable) {
  return compile_block(bc, gab_string(gab(bc), "anonymous"));
}

int compile_condexp(struct bc *bc, bool assignable, uint8_t jump_op) {
  size_t t = bc->offset - 1;

  uint64_t j = push_jump((bc), jump_op, t);

  push_scope(bc);

  int phantom = add_local(bc, gab_string(gab(bc), ""), 0);

  if (phantom < 0)
    return COMP_ERR;

  init_local(bc, phantom);

  push_storel(bc, phantom, t);

  push_pop(bc, 1, t);

  if (compile_expressions(bc) < 0)
    return COMP_ERR;

  push_pop((bc), 1, t);

  pop_slot(bc, 1);

  push_loadl((bc), phantom, t);

  push_slot(bc, 1);

  pop_scope(bc);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  patch_jump((bc), j);

  return COMP_OK;
}

int compile_exp_then(struct bc *bc, bool assignable) {
  return compile_condexp(bc, assignable, OP_JUMP_IF_FALSE);
}

int compile_exp_else(struct bc *bc, bool assignable) {
  return compile_condexp(bc, assignable, OP_JUMP_IF_TRUE);
}

int compile_exp_mch(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  uint64_t next = 0;
  v_uint64_t done_jumps = {0};

  while (!match_and_eat_token(bc, TOKEN_ELSE)) {
    if (next != 0)
      patch_jump((bc), next);

    // Duplicate the pattern
    push_op((bc), OP_DUP, t);
    push_slot(bc, 1);

    // Compile a test expression
    if (compile_expression(bc) < 0)
      return COMP_ERR;

    size_t t = bc->offset - 1;

    int m = add_message_constant(bc, gab_string(gab(bc), mGAB_EQ));

    // Test for equality
    push_send(bc, m, 1, false, t);
    pop_slot(bc, 1); // Pops 2, pushes 1

    next = push_jump((bc), OP_POP_JUMP_IF_FALSE, bc->offset - 1);
    push_pop(bc, 1, t); // Pop the test and pattern
    pop_slot(bc, 2);

    if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
      return COMP_ERR;

    if (optional_newline(bc) < 0)
      return COMP_ERR;

    if (compile_expressions(bc) < 0)
      return COMP_ERR;

    // We need to pretend to pop the slot
    // In all of the branches, because in reality
    // we only ever keep one.
    pop_slot(bc, 1);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    // Push a jump out of the match statement at the end of every case.
    v_uint64_t_push(&done_jumps, push_jump((bc), OP_JUMP, t));
  }

  // If none of the cases match, the last jump should end up here.
  patch_jump((bc), next);

  // Pop the pattern that we never matched
  push_pop((bc), 1, t);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expressions(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  for (int i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    patch_jump((bc), v_uint64_t_val_at(&done_jumps, i));
  }

  v_uint64_t_destroy(&done_jumps);

  return COMP_OK;
}

int compile_exp_bin(struct bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);
  size_t t = bc->offset - 1;

  uint16_t m;

  switch (op) {
  case TOKEN_MINUS:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_SUB));
    break;

  case TOKEN_PLUS:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_ADD));
    break;

  case TOKEN_STAR:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_MUL));
    break;

  case TOKEN_SLASH:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_DIV));
    break;

  case TOKEN_PERCENT:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_MOD));
    break;

  case TOKEN_PIPE:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_BOR));
    break;

  case TOKEN_AMPERSAND:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_BND));
    break;

  case TOKEN_EQUAL_EQUAL:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_EQ));
    break;

  case TOKEN_LESSER:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_LT));
    break;

  case TOKEN_LESSER_EQUAL:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_LTE));
    break;

  case TOKEN_LESSER_LESSER:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_LSH));
    break;

  case TOKEN_GREATER:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_GT));
    break;

  case TOKEN_GREATER_EQUAL:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_GTE));
    break;

  case TOKEN_GREATER_GREATER:
    m = add_message_constant(bc, gab_string(gab(bc), mGAB_RSH));
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

  push_send(bc, m, 1, false, t);
  return VAR_EXP;
}

int compile_exp_una(struct bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);

  size_t t = bc->offset - 1;

  int result = compile_exp_prec(bc, kUNARY);

  if (result < 0)
    return COMP_ERR;

  switch (op) {
  case TOKEN_MINUS:
    push_op((bc), OP_NEGATE, t);
    return COMP_OK;

  case TOKEN_NOT:
    push_op((bc), OP_NOT, t);
    return COMP_OK;

  case TOKEN_QUESTION:
    push_op((bc), OP_TYPE, t);
    return COMP_OK;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling unary expression");
    return COMP_ERR;
  }
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
  push_loadk((bc), gab_number(num), bc->offset - 1);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_bool(struct bc *bc, bool assignable) {
  push_op((bc), prev_tok(bc) == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE,
          bc->offset - 1);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_nil(struct bc *bc, bool assignable) {
  push_op((bc), OP_PUSH_NIL, bc->offset - 1);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_def(struct bc *bc, bool assignable) {
  size_t help_cmt_line = prev_line(bc);
  // 1 indexed, so correct for that if not the first line
  if (help_cmt_line > 0)
    help_cmt_line--;

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

  if (compile_definition(bc, name) < 0)
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

      init_local(bc, pad_local);
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
    push_loadl((bc), local, bc->offset - 1);

    push_slot(bc, 1);

    break;

  default:
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   "While compiling 'implicit parameter' expression");
    return COMP_ERR;
  }

  return COMP_OK;
}

int compile_exp_splt(struct bc *bc, bool assignable) {
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

        if (ctx >= 0) {
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
    push_loadl((bc), index, bc->offset - 1);
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    push_loadu((bc), index, bc->offset - 1);
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

  uint16_t m = add_message_constant(bc, gab_string(gab(bc), mGAB_GET));
  push_send(bc, m, 1, false, t);

  pop_slot(bc, 2);
  push_slot(bc, 1);

  return VAR_EXP;
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

  if (flags & fHAS_STRING || match_and_eat_token(bc, TOKEN_SYMBOL) ||
      match_and_eat_token(bc, TOKEN_STRING) ||
      match_and_eat_token(bc, TOKEN_INTERPOLATION_BEGIN)) {
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
    if (compile_block(bc, gab_string(gab(bc), "anonymous")) < 0)
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

  push_op((bc), OP_PUSH_UNDEFINED, t);

  push_slot(bc, 1);

  bool mv;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  push_send(bc, m, result, mv, t);

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_amp(struct bc *bc, bool assignable) {
  if (match_and_eat_token(bc, TOKEN_MESSAGE)) {
    gab_value val_name = trim_prev_id(bc);

    push_loadk(bc, gab_message(gab(bc), val_name), bc->offset - 1);

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

  push_loadk(bc, gab_message(gab(bc), gab_string(gab(bc), msg)),
             bc->offset - 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_exp_dyn(struct bc *bc, bool assignable) {
  if (expect_token(bc, TOKEN_LPAREN) < 0)
    return COMP_ERR;

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  push_dynsend(bc, result, mv, t);

  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_snd(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  gab_value name = trim_prev_id(bc);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      return compile_assignment(bc, (struct lvalue){
                                        .kind = kPROP,
                                        .slot = peek_slot(bc),
                                        .as.property = name,
                                    });
    } else if (match_ctx(bc, kASSIGNMENT_TARGET)) {
      eat_token(bc);
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    }
  }

  uint16_t m = add_message_constant(bc, name);

  bool mv = false;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1);

  push_send(bc, m, result, mv, t);

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

  uint16_t msg = add_message_constant(bc, gab_string(gab(bc), mGAB_CALL));

  push_send(bc, msg, result, mv, t);

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
  uint16_t msg = add_message_constant(bc, gab_string(gab(bc), mGAB_CALL));
  push_send(bc, msg, result, mv, t);
  return VAR_EXP;
}

int compile_exp_symcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_STRING);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(bc, gab_string(gab(bc), mGAB_CALL));

  push_send(bc, msg, result, mv, t);

  return VAR_EXP;
}

int compile_exp_scal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_STRING);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(bc, gab_string(gab(bc), mGAB_CALL));

  push_send(bc, msg, result, mv, t);

  return VAR_EXP;
}

int compile_exp_rcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_BRACK);

  pop_slot(bc, 1);

  uint16_t msg = add_message_constant(bc, gab_string(gab(bc), mGAB_CALL));

  push_send(bc, msg, result, mv, t);

  return VAR_EXP;
}

int compile_exp_and(struct bc *bc, bool assignable) {
  uint64_t end_jump = push_jump(bc, OP_LOGICAL_AND, bc->offset - 1);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(bc, kAND) < 0)
    return COMP_ERR;

  patch_jump(bc, end_jump);

  return COMP_OK;
}

int compile_exp_in(struct bc *bc, bool assignable) {
  return optional_newline(bc);
}

int compile_exp_or(struct bc *bc, bool assignable) {
  uint64_t end_jump = push_jump(bc, OP_LOGICAL_OR, bc->offset - 1);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(bc, kOR) < 0)
    return COMP_ERR;

  patch_jump(bc, end_jump);

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

  push_ctx(bc, kLOOP);

  size_t t = bc->offset - 1;

  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  uint8_t local_start = f->next_local;
  uint8_t nlooplocals = 0;
  int result;

  int mv = -1;

  do {
    switch (match_and_eat_token(bc, TOKEN_STAR)) {
    case COMP_OK:
      mv = nlooplocals;
      [[fallthrough]];
    case COMP_TOKEN_NO_MATCH: {
      if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
        return COMP_ERR;

      gab_value val_name = prev_id(bc);

      int loc = compile_local(bc, val_name, 0);

      if (loc < 0)
        return COMP_ERR;

      init_local(bc, loc);
      nlooplocals++;
      break;
    }

    default:
      return COMP_ERR;
    }
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

  init_local(bc, add_local(bc, gab_string(gab(bc), ""), 0));

  if (expect_token(bc, TOKEN_IN) < 0)
    return COMP_ERR;

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  if (!patch_mv(bc, VAR_EXP)) {
    push_op(bc, OP_VAR, t);
    push_byte(bc, 1, t);
  }

  uint64_t loop = push_loop(bc, t);

  if (mv >= 0)
    push_pack(bc, mv, nlooplocals - mv, t);

  uint64_t jump_start = push_iter(bc, nlooplocals, local_start, t);

  if (compile_expressions(bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1, t);
  pop_slot(bc, 1);

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  push_next(bc, local_start + nlooplocals, t);

  patch_loop(bc, loop, t);

  pop_scope(bc); /* Pop the scope once, after we exit the loop. */

  patch_jump(bc, jump_start);

  push_op(bc, OP_PUSH_NIL, t);
  push_slot(bc, 1);

  if (pop_ctxloop(bc) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_exp_brk(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  int ctx = peek_ctx(bc, kLOOP, 0);

  if (ctx < 0) {
    compiler_error(bc, GAB_BREAK_OUTSIDE_LOOP, "");
    return COMP_ERR;
  }

  if (!curr_prefix(bc)) {
    push_op(bc, OP_PUSH_NIL, t);
    goto fin;
  }

  if (compile_expression(bc) < 0)
    return COMP_OK;

fin : {
  size_t jump = push_jump(bc, OP_JUMP, t);
  v_uint64_t_push(&bc->contexts[ctx].as.break_list, jump);
  return COMP_OK;
}
}

int compile_exp_lop(struct bc *bc, bool assignable) {
  push_scope(bc);

  size_t t = bc->offset - 1;

  uint64_t loop = push_loop(bc, t);

  push_ctx(bc, kLOOP);

  if (compile_expressions(bc) < 0)
    return COMP_ERR;

  push_pop(bc, 1, t);

  if (match_and_eat_token(bc, TOKEN_UNTIL)) {
    if (compile_expression(bc) < 0)
      return COMP_ERR;

    size_t t = bc->offset - 1;

    uint64_t jump = push_jump(bc, OP_POP_JUMP_IF_TRUE, t);

    patch_loop(bc, loop, t);

    patch_jump(bc, jump);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    patch_loop(bc, loop, t);
  }

  push_op(bc, OP_PUSH_NIL, t);

  if (pop_ctxloop(bc) < 0)
    return COMP_ERR;

  pop_scope(bc);
  return COMP_OK;
}

int compile_exp_sym(struct bc *bc, bool assignable) {
  return compile_symbol(bc);
}

int compile_exp_yld(struct bc *bc, bool assignable) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (!get_rule(curr_tok(bc)).prefix) {
    gab_value proto =
        gab_sprototype(gab(bc), bc->src, f->name, bc->src->bytecode.len + 4, 1);

    uint16_t kproto = addk(bc, proto);

    push_yield(bc, kproto, 0, false, bc->offset - 1);

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

  gab_value proto =
      gab_sprototype(gab(bc), bc->src, f->name, bc->src->bytecode.len + 4, 1);

  uint16_t kproto = addk(bc, proto);

  push_slot(bc, 1);

  push_yield(bc, kproto, result, mv, bc->offset - 1);

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_rtn(struct bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    push_slot(bc, 1);

    push_op(bc, OP_PUSH_NIL, bc->offset - 1);

    push_ret(bc, 1, false, bc->offset - 1);

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

  push_ret(bc, result, mv, bc->offset - 1);

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
    PREFIX(brk),                       // BREAK
    INFIX(bin, TERM, false),                  // PLUS
    PREFIX_INFIX(una, bin, TERM, false),      // MINUS
    PREFIX_INFIX(splt, bin, FACTOR, false),                // STAR
    INFIX(bin, FACTOR, false),                // SLASH
    INFIX(bin, FACTOR, false),                // PERCENT
    NONE(),                            // COMMA
    INFIX(dyn, SEND, false),       // COLON
    PREFIX_INFIX(amp, bin, BITWISE_AND, false),           // AMPERSAND
    NONE(),           // DOLLAR
    PREFIX_INFIX(sym, symcal, SEND, false), // SYMBOL
    PREFIX_INFIX(emp, snd, SEND, true), // MESSAGE
    NONE(),              // DOT
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
  push_ctxframe(bc, name);

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;

  if (curr_tok(bc) == TOKEN_EOF)
    return gab_undefined;

  for (uint8_t i = 0; i < narguments; i++) {
    init_local(bc, add_local(bc, arguments[i], 0));
  }

  if (compile_expressions_body(bc) < 0)
    return gab_undefined;

  bool mv = patch_mv(bc, VAR_EXP);

  push_ret(bc, !mv, mv, bc->offset - 1);

  gab_value p = pop_ctxframe(bc);

  if (p == gab_undefined)
    return COMP_ERR;

  gab_value main = gab_block(gab(bc), p);

  gab_gciref(gab(bc), main);
  gab_egkeep(eg(bc), main);

  if (fGAB_DUMP_BYTECODE & bc->flags)
    gab_fmodinspect(stdout, GAB_VAL_TO_PROTOTYPE(p));

  return main;
}

gab_value gab_cmpl(struct gab_triple gab, struct gab_cmpl_argt args) {
  struct gab_src *src =
      gab_src(gab.eg, (char *)args.source, strlen(args.source) + 1);

  struct bc bc;
  bc_create(&bc, gab, src, args.flags);

  gab_value vargv[args.len];

  for (int i = 0; i < args.len; i++) {
    vargv[i] = gab_string(gab, args.argv[i]);
  }

  gab_value module = compile(&bc, gab_string(gab, args.name), args.len, vargv);

  bc_destroy(&bc);

  return module;
}

static void compiler_error(struct bc *bc, enum gab_status e,
                           const char *note_fmt, ...) {
  if (bc->panic)
    return;

  bc->panic = true;

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (bc->flags & fGAB_DUMP_ERROR) {
    va_list va;
    va_start(va, note_fmt);

    gab_verr(
        (struct gab_err_argt){
            .src = bc->src,
            .context = f->name,
            .status = e,
            .tok = bc->offset - 1,
            .note_fmt = note_fmt,
        },
        va);

    va_end(va);
  }
}

uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                         uint64_t offset);

uint64_t dumpSimpleInstruction(FILE *stream, struct gab_obj_prototype *self,
                               uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s\n", name);
  return offset + 1;
}

uint64_t dumpDynSendInstruction(FILE *stream, struct gab_obj_prototype *self,
                                uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];

  uint8_t have = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  uint8_t want = v_uint8_t_val_at(&self->src->bytecode, offset + 4);

  uint8_t var = have & FLAG_VAR_EXP;
  have = have >> 1;

  fprintf(stream,
          "%-25s"
          "(%s%d) -> %d\n",
          name, var ? "& more" : "", have, want);
  return offset + 29;
}

uint64_t dumpSendInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];

  uint16_t constant =
      ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 1)) << 8 |
      v_uint8_t_val_at(&self->src->bytecode, offset + 2);

  gab_value msg = v_gab_value_val_at(&self->src->constants, constant);

  uint8_t have = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  uint8_t want = v_uint8_t_val_at(&self->src->bytecode, offset + 4);

  uint8_t var = have & FLAG_VAR_EXP;
  have = have >> 1;

  fprintf(stream, "%-25s" ANSI_COLOR_BLUE, name);
  gab_fvalinspect(stream, msg, 0);
  fprintf(stream, ANSI_COLOR_RESET " (%s%d) -> %d\n", var ? "& more" : "", have,
          want);

  return offset + 29;
}

uint64_t dumpByteInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}
uint64_t dumpReturnInstruction(FILE *stream, struct gab_obj_prototype *self,
                               uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t have = havebyte >> 1;
  fprintf(stream, "%-25s%hhx%s\n", "RETURN", have,
          havebyte & FLAG_VAR_EXP ? " & more" : "");
  return offset + 2;
}

uint64_t dumpYieldInstruction(FILE *stream, struct gab_obj_prototype *self,
                              uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  uint8_t have = havebyte >> 1;
  fprintf(stream, "%-25s%hhx%s\n", "YIELD", have,
          havebyte & FLAG_VAR_EXP ? " & more" : "");
  return offset + 4;
}

uint64_t dumpTwoByteInstruction(FILE *stream, struct gab_obj_prototype *self,
                                uint64_t offset) {
  uint8_t operandA = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t operandB = v_uint8_t_val_at(&self->src->bytecode, offset + 2);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx %hhx\n", name, operandA, operandB);
  return offset + 3;
}

uint64_t dumpDictInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint8_t i, uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name = gab_opcode_names[i];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
};

uint64_t dumpConstantInstruction(FILE *stream, struct gab_obj_prototype *self,
                                 uint64_t offset) {
  uint16_t constant =
      ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 1)) << 8 |
      v_uint8_t_val_at(&self->src->bytecode, offset + 2);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s", name);
  gab_fvalinspect(stdout, v_gab_value_val_at(&self->src->constants, constant),
                  0);
  fprintf(stream, "\n");
  return offset + 3;
}

uint64_t dumpJumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint64_t sign, uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  uint16_t dist = (uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 1)
                  << 8;
  dist |= v_uint8_t_val_at(&self->src->bytecode, offset + 2);

  fprintf(stream,
          "%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET "\n",
          name, offset, offset + 3 + (sign * (dist)));
  return offset + 3;
}

uint64_t dumpIter(FILE *stream, struct gab_obj_prototype *self,
                  uint64_t offset) {
  uint16_t dist = (uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 3)
                  << 8;
  dist |= v_uint8_t_val_at(&self->src->bytecode, offset + 4);

  uint8_t want = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t start = v_uint8_t_val_at(&self->src->bytecode, offset + 2);

  fprintf(stream,
          "%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " %03d locals from %03d\n",
          "ITER", offset, offset + 5 + dist, want, start);

  return offset + 5;
}

uint64_t dumpNext(FILE *stream, struct gab_obj_prototype *self,
                  uint64_t offset) {
  fprintf(stream, "%-25s\n", "NEXT");
  return offset + 2;
}

uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                         uint64_t offset) {
  uint8_t op = v_uint8_t_val_at(&self->src->bytecode, offset);
  switch (op) {
  case OP_PUSH_UNDEFINED:
  case OP_STORE_LOCAL_0:
  case OP_STORE_LOCAL_1:
  case OP_STORE_LOCAL_2:
  case OP_STORE_LOCAL_3:
  case OP_STORE_LOCAL_4:
  case OP_STORE_LOCAL_5:
  case OP_STORE_LOCAL_6:
  case OP_STORE_LOCAL_7:
  case OP_STORE_LOCAL_8:
  case OP_POP_STORE_LOCAL_0:
  case OP_POP_STORE_LOCAL_1:
  case OP_POP_STORE_LOCAL_2:
  case OP_POP_STORE_LOCAL_3:
  case OP_POP_STORE_LOCAL_4:
  case OP_POP_STORE_LOCAL_5:
  case OP_POP_STORE_LOCAL_6:
  case OP_POP_STORE_LOCAL_7:
  case OP_POP_STORE_LOCAL_8:
  case OP_LOAD_LOCAL_0:
  case OP_LOAD_LOCAL_1:
  case OP_LOAD_LOCAL_2:
  case OP_LOAD_LOCAL_3:
  case OP_LOAD_LOCAL_4:
  case OP_LOAD_LOCAL_5:
  case OP_LOAD_LOCAL_6:
  case OP_LOAD_LOCAL_7:
  case OP_LOAD_LOCAL_8:
  case OP_LOAD_UPVALUE_0:
  case OP_LOAD_UPVALUE_1:
  case OP_LOAD_UPVALUE_2:
  case OP_LOAD_UPVALUE_3:
  case OP_LOAD_UPVALUE_4:
  case OP_LOAD_UPVALUE_5:
  case OP_LOAD_UPVALUE_6:
  case OP_LOAD_UPVALUE_7:
  case OP_LOAD_UPVALUE_8:
  case OP_PUSH_FALSE:
  case OP_PUSH_NIL:
  case OP_PUSH_TRUE:
  case OP_SWAP:
  case OP_DUP:
  case OP_NOT:
  case OP_MATCH:
  case OP_POP:
  case OP_TYPE:
  case OP_NOP:
    return dumpSimpleInstruction(stream, self, offset);
  case OP_PACK:
    return dumpTwoByteInstruction(stream, self, offset);
  case OP_LOGICAL_AND:
  case OP_JUMP_IF_FALSE:
  case OP_JUMP_IF_TRUE:
  case OP_JUMP:
  case OP_POP_JUMP_IF_FALSE:
  case OP_POP_JUMP_IF_TRUE:
  case OP_LOGICAL_OR:
    return dumpJumpInstruction(stream, self, 1, offset);
  case OP_LOOP:
    return dumpJumpInstruction(stream, self, -1, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(stream, self, offset);
  case OP_ITER:
    return dumpIter(stream, self, offset);
  case OP_SEND_ANA:
  case OP_SEND_MONO_BLOCK:
  case OP_SEND_MONO_NATIVE:
  case OP_SEND_MONO_PROPERTY:
  case OP_SEND_PRIMITIVE_CONCAT:
  case OP_SEND_PRIMITIVE_ADD:
  case OP_SEND_PRIMITIVE_SUB:
  case OP_SEND_PRIMITIVE_MUL:
  case OP_SEND_PRIMITIVE_DIV:
  case OP_SEND_PRIMITIVE_MOD:
  case OP_SEND_PRIMITIVE_EQ:
  case OP_SEND_PRIMITIVE_LT:
  case OP_SEND_PRIMITIVE_LTE:
  case OP_SEND_PRIMITIVE_GT:
  case OP_SEND_PRIMITIVE_GTE:
  case OP_SEND_PRIMITIVE_CALL_BLOCK:
  case OP_SEND_PRIMITIVE_CALL_NATIVE:
  case OP_SEND_PRIMITIVE_CALL_SUSPENSE:
    return dumpSendInstruction(stream, self, offset);
  case OP_DYNAMIC_SEND:
    return dumpDynSendInstruction(stream, self, offset);
  case OP_POP_N:
  case OP_STORE_LOCAL:
  case OP_POP_STORE_LOCAL:
  case OP_LOAD_UPVALUE:
  case OP_INTERPOLATE:
  case OP_SHIFT:
  case OP_NEXT:
  case OP_VAR:
  case OP_LOAD_LOCAL: {
    return dumpByteInstruction(stream, self, offset);
  }
  case OP_RETURN:
    return dumpReturnInstruction(stream, self, offset);
  case OP_YIELD: {
    return dumpYieldInstruction(stream, self, offset);
  }
  case OP_MESSAGE: {
    offset++;
    uint16_t proto_constant =
        ((((uint16_t)self->src->bytecode.data[offset]) << 8) |
         self->src->bytecode.data[offset + 1]);
    offset += 4;

    gab_value pval = v_gab_value_val_at(&self->src->constants, proto_constant);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    struct gab_obj_string *func_name = GAB_VAL_TO_STRING(p->name);

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_MESSAGE",
           (int)func_name->len, func_name->data);

    for (int j = 0; j < p->as.block.nupvalues; j++) {
      uint8_t flags = p->data[j * 2];
      uint8_t index = p->data[j * 2 + 1];
      int isLocal = flags & fVAR_LOCAL;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_BLOCK: {
    offset++;
    uint16_t proto_constant =
        ((((uint16_t)self->src->bytecode.data[offset]) << 8) |
         self->data[offset + 1]);
    offset += 2;

    gab_value pval = v_gab_value_val_at(&self->src->constants, proto_constant);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    struct gab_obj_string *func_name = GAB_VAL_TO_STRING(p->name);

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_BLOCK",
           (int)func_name->len, func_name->data);

    for (int j = 0; j < p->as.block.nupvalues; j++) {
      uint8_t flags = p->data[j * 2];
      uint8_t index = p->data[j * 2 + 1];
      int isLocal = flags & fVAR_LOCAL;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_RECORD: {
    return dumpDictInstruction(stream, self, op, offset);
  }
  case OP_TUPLE: {
    return dumpDictInstruction(stream, self, op, offset);
  }
  default: {
    uint8_t code = v_uint8_t_val_at(&self->src->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

int gab_fmodinspect(FILE *stream, struct gab_obj_prototype *proto) {
  uint64_t offset = 0;
  while (offset < proto->src->bytecode.len) {
    fprintf(stream, ANSI_COLOR_YELLOW "%04lu " ANSI_COLOR_RESET, offset);
    offset = dumpInstruction(stream, proto, offset);
  }

  return 0;
}
