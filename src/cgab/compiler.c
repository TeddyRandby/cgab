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
  v_uint8_t bc;
  v_uint64_t bc_toks;
  gab_value name;

  size_t prev_op_at;
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
  kFRAME,
  kTUPLE,
  kASSIGNMENT_TARGET,
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

  size_t next_block;

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
  kUNARY,       // - not
  kSEND,        // ( { :
  kPRIMARY
};

typedef int (*compile_f)(struct bc *, bool);
struct compile_rule {
  compile_f prefix;
  compile_f infix;
  enum prec_k prec;
  bool multi_line;
};

void bc_create(struct bc *self, struct gab_triple gab, struct gab_src *source) {
  memset(self, 0, sizeof(*self));

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

#define FMT_UNEXPECTEDTOKEN "Expected $ instead."
static void compiler_error(struct bc *bc, enum gab_status e,
                           const char *help_fmt, ...);

static bool match_token(struct bc *bc, gab_token tok);

static int eat_token(struct bc *bc);

//------------------- Token Helpers -----------------------
size_t prev_line(struct bc *bc) {
  return v_uint64_t_val_at(&bc->src->token_lines, bc->offset - 1);
}

gab_token curr_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset);
}

gab_token prev_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset - 1);
}

s_char curr_src(struct bc *bc) {
  return v_s_char_val_at(&bc->src->token_srcs, bc->offset);
}

s_char prev_src(struct bc *bc) {
  return v_s_char_val_at(&bc->src->token_srcs, bc->offset - 1);
}
static inline bool match_token(struct bc *bc, gab_token tok) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset) == tok;
}

static inline bool match_terminator(struct bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL) || match_token(bc, TOKEN_EOF);
}

static int eat_token(struct bc *bc) {
  if (match_token(bc, TOKEN_EOF)) {
    compiler_error(bc, GAB_UNEXPECTED_EOF, "");
    return COMP_ERR;
  }

  bc->offset++;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    compiler_error(bc, GAB_MALFORMED_TOKEN,
                   "This token is malformed or unrecognized.");
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
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, FMT_UNEXPECTEDTOKEN,
                   gab_string(gab(bc), gab_token_names[tok]));
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

    /* We don't peek to parent frame contexts, unless we're peeking for a frame
     * context */
    if (kind != kFRAME && bc->contexts[idx].kind == kFRAME)
      break;

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

  f->prev_op_at = v_uint8_t_push(&f->bc, op);
  v_uint64_t_push(&f->bc_toks, t);
}

static inline void push_byte(struct bc *bc, uint8_t data, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  v_uint8_t_push(&f->bc, data);
  v_uint64_t_push(&f->bc_toks, t);
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

static inline uint16_t addk(struct bc *bc, gab_value value) {
  gab_iref(gab(bc), value);
  gab_egkeep(eg(bc), value);

  assert(bc->src->constants.len < UINT16_MAX);

  return v_gab_value_push(&bc->src->constants, value);
}

static inline void push_shift(struct bc *bc, uint8_t n, size_t t) {
  if (n <= 1)
    return; // No op

  push_op(bc, OP_SHIFT, t);
  push_byte(bc, n, t);
}

static inline void push_k(struct bc *bc, uint16_t k, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->curr_bb == f->prev_bb) {
    switch (f->prev_op) {
    case OP_CONSTANT: {
      size_t prev_local_arg = f->prev_op_at + 1;

      uint8_t prev_ka = v_uint8_t_val_at(&f->bc, prev_local_arg);
      uint8_t prev_kb = v_uint8_t_val_at(&f->bc, prev_local_arg + 1);

      uint16_t prev_k = prev_ka << 8 | prev_kb;

      v_uint8_t_pop(&f->bc);
      v_uint64_t_pop(&f->bc_toks);
      v_uint8_t_pop(&f->bc);
      v_uint64_t_pop(&f->bc_toks);

      f->prev_op = OP_NCONSTANT;
      v_uint8_t_set(&f->bc, f->prev_op_at, OP_NCONSTANT);

      push_byte(bc, 2, t);
      push_short(bc, prev_k, t);
      push_short(bc, k, t);

      return;
    }
    case OP_NCONSTANT: {
      size_t prev_local_arg = f->prev_op_at + 1;
      uint8_t prev_n = v_uint8_t_val_at(&f->bc, prev_local_arg);
      v_uint8_t_set(&f->bc, prev_local_arg, prev_n + 1);
      push_short(bc, k, t);
      return;
    }
    }
  }
#endif

  push_op(bc, OP_CONSTANT, t);
  push_short(bc, k, t);
}

static inline void push_loadi(struct bc *bc, enum gab_kind k, size_t t) {
  assert(k == kGAB_UNDEFINED || k == kGAB_NIL || k == kGAB_TRUE ||
         k == kGAB_FALSE);

  push_k(bc, k - 1, t);
};

static inline void push_loadk(struct bc *bc, gab_value k, size_t t) {
  push_k(bc, addk(bc, k), t);
}

static inline void push_loadl(struct bc *bc, uint8_t local, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->curr_bb == f->prev_bb) {
    switch (f->prev_op) {
    case OP_LOAD_LOCAL: {
      size_t prev_local_arg = f->prev_op_at + 1;
      uint8_t prev_local = v_uint8_t_val_at(&f->bc, prev_local_arg);
      push_byte(bc, prev_local, t);
      push_byte(bc, local, t);
      v_uint8_t_set(&f->bc, prev_local_arg, 2);
      v_uint8_t_set(&f->bc, f->prev_op_at, OP_NLOAD_LOCAL);
      f->prev_op = OP_NLOAD_LOCAL;
      return;
    }
    case OP_NLOAD_LOCAL: {
      size_t prev_local_arg = f->prev_op_at + 1;
      uint8_t old_arg = v_uint8_t_val_at(&f->bc, prev_local_arg);
      v_uint8_t_set(&f->bc, prev_local_arg, old_arg + 1);
      push_byte(bc, local, t);
      return;
    }
    }
  }
#endif

  push_op(bc, OP_LOAD_LOCAL, t);
  push_byte(bc, local, t);
  return;
}

static inline void push_loadu(struct bc *bc, uint8_t upv, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->curr_bb == f->prev_bb) {
    switch (f->prev_op) {
    case OP_LOAD_UPVALUE: {
      size_t prev_upv_arg = f->prev_op_at + 1;
      uint8_t prev_upv = v_uint8_t_val_at(&f->bc, prev_upv_arg);
      push_byte(bc, prev_upv, t);
      push_byte(bc, upv, t);
      v_uint8_t_set(&f->bc, prev_upv_arg, 2);
      v_uint8_t_set(&f->bc, f->prev_op_at, OP_NLOAD_UPVALUE);
      f->prev_op = OP_NLOAD_UPVALUE;
      return;
    }
    case OP_NLOAD_UPVALUE: {
      size_t prev_upv_arg = f->prev_op_at + 1;
      uint8_t old_arg = v_uint8_t_val_at(&f->bc, prev_upv_arg);
      v_uint8_t_set(&f->bc, prev_upv_arg, old_arg + 1);
      push_byte(bc, upv, t);
      return;
    }
    }
  }
#endif

  push_op(bc, OP_LOAD_UPVALUE, t);
  push_byte(bc, upv, t);
  return;
}

static inline void push_storel(struct bc *bc, uint8_t local, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->curr_bb == f->prev_bb) {
    switch (f->prev_op) {
    case OP_POPSTORE_LOCAL: {
      size_t prev_local_arg = f->prev_op_at + 1;
      uint8_t prev_local = v_uint8_t_val_at(&f->bc, prev_local_arg);
      push_byte(bc, prev_local, t);
      push_byte(bc, local, t);
      v_uint8_t_set(&f->bc, prev_local_arg, 2);
      v_uint8_t_set(&f->bc, f->prev_op_at, OP_NPOPSTORE_STORE_LOCAL);
      f->prev_op = OP_NPOPSTORE_STORE_LOCAL;
      return;
    }
    case OP_NPOPSTORE_LOCAL: {
      size_t prev_loc_arg = f->prev_op_at + 1;
      uint8_t old_arg = v_uint8_t_val_at(&f->bc, prev_loc_arg);
      v_uint8_t_set(&f->bc, prev_loc_arg, old_arg + 1);

      push_byte(bc, local, t);

      v_uint8_t_set(&f->bc, f->prev_op_at, OP_NPOPSTORE_STORE_LOCAL);
      f->prev_op = OP_NPOPSTORE_STORE_LOCAL;
      return;
    }
    }
  }
#endif

  push_op(bc, OP_STORE_LOCAL, t);
  push_byte(bc, local, t);
  return;
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

static inline void push_block(struct bc *bc, gab_value p, size_t t) {
  assert(gab_valkind(p) == kGAB_BPROTOTYPE);
  struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(p);

  if (proto->as.block.nupvalues == 0) {
    gab_value b = gab_block(gab(bc), p);
    push_loadk(bc, b, t);
    return;
  }

  push_op(bc, OP_BLOCK, t);
  push_short(bc, addk(bc, p), t);
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

static inline bool patch_trim(struct bc *bc, uint8_t want) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->prev_bb != f->curr_bb)
    return false;

  switch (f->prev_op) {
  case OP_TRIM:
    if (want != VAR_EXP) {
      v_uint8_t_set(&f->bc, f->bc.len - 1, want); /* want */
    } else {
      f->bc.len -= 2;
      f->bc_toks.len -= 2;
    }
    return true;
  }

  return false;
}

static inline void push_dynsend(struct bc *bc, uint8_t have, bool mv,
                                size_t t) {
  assert(have < 16);

  push_op(bc, OP_DYNSEND, t);
  push_nnop(bc, 2, t);
  push_byte(bc, encode_arity(1 + have, mv), t);
}

static inline void push_send(struct bc *bc, gab_value m, uint8_t have, bool mv,
                             size_t t) {
  assert(have < 16);

  if (gab_valkind(m) == kGAB_STRING) {
    m = gab_message(gab(bc), m);
  }

  assert(gab_valkind(m) == kGAB_MESSAGE);

  uint16_t ks = addk(bc, m);
  addk(bc, gab_undefined);
  addk(bc, gab_undefined);
  addk(bc, gab_undefined);

  if (have == 0 && !mv && patch_trim(bc, VAR_EXP)) {
    mv = true;
  } else {
    have++;
  }

  push_op(bc, OP_SEND, t);
  push_short(bc, ks, t);
  push_byte(bc, encode_arity(have, mv), t);
}

static inline void push_pop(struct bc *bc, uint8_t n, size_t t) {
  if (n > 1) {
    push_op(bc, OP_POP_N, t);
    push_byte(bc, n, t);
    return;
  }

#if cGAB_SUPERINSTRUCTIONS
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->curr_bb == f->prev_bb) {
    switch (f->prev_op) {
    case OP_STORE_LOCAL:
      f->prev_op_at = f->bc.len - 2;
      f->bc.data[f->prev_op_at] = OP_POPSTORE_LOCAL;
      f->prev_op = OP_POPSTORE_LOCAL;
      return;

    case OP_NPOPSTORE_STORE_LOCAL:
      f->bc.data[f->prev_op_at] = OP_NPOPSTORE_LOCAL;
      f->prev_op = OP_NPOPSTORE_LOCAL;
      return;

    default:
      break;
    }
  }
#endif

  push_op(bc, OP_POP, t);
}

static inline void push_trim(struct bc *bc, uint8_t want, size_t t) {
  push_op(bc, OP_TRIM, t);
  push_byte(bc, 1, t);
}

static inline void push_pack(struct bc *bc, uint8_t have, bool mv,
                             uint8_t below, uint8_t above, size_t t) {
  push_op(bc, OP_PACK, t);
  push_byte(bc, encode_arity(have, mv), t);
  push_byte(bc, below, t);
  push_byte(bc, above, t);
}

static inline size_t push_jump(struct bc *bc, uint8_t op, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  push_op(bc, op, t);
  push_nnop(bc, 2, t);

  return f->bc.len - 2;
}

static inline void patch_jump(struct bc *bc, size_t jump) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->curr_bb++;

  size_t dist = f->bc.len - jump - 2;

  assert(dist < UINT16_MAX);

  v_uint8_t_set(&f->bc, jump, (dist >> 8) & 0xff);
  v_uint8_t_set(&f->bc, jump + 1, dist & 0xff);
}

static inline size_t push_loop(struct bc *bc, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  return f->bc.len;
}

static inline void patch_loop(struct bc *bc, size_t loop, size_t t) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->curr_bb++;

  size_t dist = f->bc.len - loop + 3;
  assert(dist < UINT16_MAX);
  push_op(bc, OP_LOOP, t);
  push_short(bc, dist, t);
}

gab_value curr_id(struct bc *bc) {
  s_char s = curr_src(bc);

  gab_value sv = gab_nstring(gab(bc), s.len, s.data);

  return sv;
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
    assert(false && "Too many slots. This is an internal compiler error.");
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
    assert(false && "Too many slots. This is an internal compiler error.");
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
    assert(false && "Too few slots. This is an internal compiler error.");
  }

  f->next_slot -= n;

  printf("[POP] -%d -> %d |%s:%d\n", n, f->next_slot, file, line);
}

#else

static inline void pop_slot(struct bc *bc, uint16_t n) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->next_slot - n < 0) {
    assert(false && "Too few slots. This is an internal compiler error.");
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
    compiler_error(
        bc, GAB_TOO_MANY_LOCALS,
        "For arbitrary reasons, blocks cannot decalre more than 255 locals.");
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
    compiler_error(bc, GAB_TOO_MANY_UPVALUES,
                   "For arbitrary reasons, blocks cannot capture more than 255 "
                   "variables.");
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
        compiler_error(
            bc, GAB_REFERENCE_BEFORE_INITIALIZE,
            "The variable $ was referenced before it was initialized.", name);
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
      compiler_error(bc, GAB_CAPTURED_MUTABLE,
                     "Variables are mutable by default.\n"
                     "To capture $, declare it as immutable:\n"
                     "\n | " ANSI_COLOR_MAGENTA "def" ANSI_COLOR_RESET
                     " $ = ...\n",
                     name, name);
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

static void push_ctxframe(struct bc *bc, gab_value name, bool is_message) {
  int ctx = push_ctx(bc, kFRAME);

  assert(ctx >= 0 && "Failed to push frame context");

  struct context *c = bc->contexts + ctx;

  struct frame *f = &c->as.frame;

  memset(f, 0, sizeof(struct frame));

  bool is_anonymous = name == gab_nil;

  f->name = is_anonymous ? gab_string(gab(bc), "anonymous") : name;

  addk(bc, name);

  init_local(bc, add_local(bc,
                           is_message     ? gab_string(gab(bc), "self")
                           : is_anonymous ? gab_string(gab(bc), "")
                                          : name,
                           0));

  push_trim(bc, 0, bc->offset - 1);
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

  gab_value p =
      gab_bprototype(gab(bc), bc->src, f->name, bc->next_block, f->bc.len,
                     (struct gab_blkproto_argt){
                         .nslots = nslots,
                         .nlocals = nlocals,
                         .narguments = nargs,
                         .nupvalues = nupvalues,
                         .flags = f->upv_flags,
                         .indexes = f->upv_indexes,
                     });

  gab_iref(gab(bc), p);
  gab_egkeep(eg(bc), p);

  assert(match_ctx(bc, kFRAME));

  if (pop_ctx(bc, kFRAME) < 0)
    return gab_undefined;

  if (f->bc.data[0] == OP_TRIM)
    v_uint8_t_set(&f->bc, 1, nlocals);

  bc->next_block =
      gab_srcappend(bc->src, f->bc.len, f->bc.data, f->bc_toks.data);

  v_uint8_t_destroy(&f->bc);
  v_uint64_t_destroy(&f->bc_toks);

  return p;
}

struct compile_rule get_rule(gab_token k);
int compile_exp_startwith(struct bc *bc, int prec, gab_token rule);
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

  // Skip the first and last bytes of the string.
  // These are the opening/closing quotes, doublequotes, or brackets (For
  // interpolation).
  for (size_t i = 1; i < raw_str.len - 1; i++) {
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
      case '"':
        buffer[buf_end++] = '"';
        break;
      case '\'':
        buffer[buf_end++] = '\'';
        break;
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
    compiler_error(bc, GAB_MALFORMED_STRING,
                   "Single quoted strings can contain interpolations.\n"
                   "\n | 'this is { an:interpolation }'\n"
                   "\nBoth single and double quoted strings can contain escape "
                   "sequences.\n"
                   "\n | 'a newline -> \\n, or a forward slash -> \\\\'"
                   "\n | \"arbitrary unicode: \\u[2502]\"");
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
  if (prev_tok(bc) == TOKEN_SIGIL)
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
  // Somehow track below and above for packing
  int below = -1, result = 0;
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
      if (below >= 0) {
        int ctx = peek_ctx(bc, kFRAME, 0);
        struct frame *f = &bc->contexts[ctx].as.frame;

        gab_value other_name = f->local_names[below + 1];

        compiler_error(
            bc, GAB_INVALID_REST_VARIABLE,
            "The parameter '$' at index $ is already a 'rest' parameter.\n"
            "\nBlocks can only declare one parameter as a 'rest' parameter.\n",
            other_name, gab_number(below));

        return COMP_ERR;
      }

      below = --narguments;
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

  if (below >= 0) {
    patch_trim(bc, VAR_EXP);
    push_pack(bc, 0, true, below + 1, narguments - below, bc->offset - 1);
  }

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = below >= 0 ? VAR_EXP : narguments;
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
  uint64_t line = prev_line(bc);

  if (compile_expressions_body(bc) < 0)
    return COMP_ERR;

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    compiler_error(bc, GAB_MISSING_END,
                   "Make sure the block at line $ is closed.",
                   gab_number(line));
    return COMP_ERR;
  }

  return COMP_OK;
}

int compile_block_body(struct bc *bc) {
  int result = compile_expressions(bc);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  pop_scope(bc);

  bool mv = patch_trim(bc, VAR_EXP);

  push_ret(bc, !mv, mv, bc->offset - 1);

  return COMP_OK;
}

int compile_message_spec(struct bc *bc) {
  if (expect_token(bc, TOKEN_LBRACE) < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_RBRACE)) {
    push_slot(bc, 1);
    push_loadi(bc, kGAB_UNDEFINED, bc->offset - 1);
    return COMP_OK;
  }

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  return COMP_OK;
}

int compile_lambda(struct bc *bc, size_t t) {
  push_ctxframe(bc, gab_nil, false);

  push_scope(bc);

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  pop_scope(bc);

  bool mv = patch_trim(bc, VAR_EXP);

  push_ret(bc, !mv, mv, bc->offset - 1);

  gab_value p = pop_ctxframe(bc);

  if (p == gab_undefined)
    return COMP_ERR;

  push_block(bc, p, bc->offset - 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_block(struct bc *bc, gab_value name) {
  push_ctxframe(bc, name, false);

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(bc) < 0)
    return COMP_ERR;

  gab_value p = pop_ctxframe(bc);

  if (p == gab_undefined)
    return COMP_ERR;

  push_block(bc, p, bc->offset - 1);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_message(struct bc *bc, gab_value name, size_t t) {
  if (compile_message_spec(bc) < 0)
    return COMP_ERR;

  push_ctxframe(bc, name, true);

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_block_body(bc) < 0)
    return COMP_ERR;

  gab_value p = pop_ctxframe(bc);

  if (p == gab_undefined)
    return COMP_ERR;

  bool is_anonymous = name == gab_nil;

  push_op(bc, is_anonymous ? OP_DYNSPEC : OP_SPEC, t);
  push_short(bc, addk(bc, p), t);

  if (!is_anonymous) {
    gab_value m = gab_message(gab(bc), name);
    push_short(bc, addk(bc, m), t);
  }

  pop_slot(bc, 1 + is_anonymous);

  push_slot(bc, 1);
  return COMP_OK;
}

int compile_expression(struct bc *bc) {
  return compile_exp_prec(bc, kASSIGNMENT);
}

int compile_tuple(struct bc *bc, uint8_t want, bool *mv_out) {
  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  uint8_t have = 0;
  bool mv = false;

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

  if (have == 0) {
    compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression.");
    return COMP_ERR;
  }

  if (want == VAR_EXP) {
    /*
     * If we want all possible values, try to patch a mv.
     * If we are successful, remove one from have.

     * values in ADDITION to the mv ending the tuple.
     */
    have -= patch_trim(bc, VAR_EXP);
  } else {
    /*
     * Here we want a specific number of values. Try to patch the mv to want
     * however many values we need in order to match up have and want. Again, we
     * subtract an extra one because in the case where we do patch, have's
     * meaning is now the number of ADDITIONAL values we have.
     */
    if (patch_trim(bc, want - have + 1)) {
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
      push_loadi(bc, kGAB_NIL, bc->offset - 1);
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

void adjust_preceding_lvalues(struct bc *bc) {
  int ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

  if (ctx >= 0) {
    v_lvalue *lvalues = &bc->contexts[ctx].as.assignment_target;

    for (size_t i = 0; i < lvalues->len; i++)
      lvalues->data[i].slot++;
  }
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
    compiler_error(bc, GAB_INVALID_REST_VARIABLE,
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

  if (n_rest_values) {
    for (uint8_t i = 0; i < lvalues->len; i++) {
      if (lvalues->data[i].kind == kEXISTING_REST_LOCAL ||
          lvalues->data[i].kind == kNEW_REST_LOCAL) {
        uint8_t before = i;
        uint8_t after = lvalues->len - i - 1;

        push_pack(bc, have, mv, before, after, t);

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
      // Fallthrough
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
      push_send(bc, lval.as.property, 1, false, t);

      if (!is_last_assignment)
        push_pop(bc, 1, t);

      pop_slot(bc, 1 + !is_last_assignment);
      break;
    }

    case kINDEX: {
      push_send(bc, gab_string(gab(bc), mGAB_SET), 2, false, t);

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
  if (match_and_eat_token(bc, TOKEN_SIGIL) ||
      match_and_eat_token(bc, TOKEN_STRING) ||
      match_and_eat_token(bc, TOKEN_INTERPOLATION_BEGIN)) {
    size_t t = bc->offset - 1;

    if (compile_string(bc) < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (compile_expression(bc) < 0)
        return COMP_ERR;
    } else {
      push_loadi(bc, kGAB_TRUE, t);
      push_slot(bc, 1);
    }

    return COMP_OK;
  }

  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {
    gab_value val_name = prev_id(bc);

    size_t t = bc->offset - 1;

    push_slot(bc, 1);

    push_loadk(bc, val_name, t);

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
        push_loadl(bc, value_in, t);
        return COMP_OK;

      case COMP_RESOLVED_TO_UPVALUE:
        push_loadu(bc, value_in, t);
        return COMP_OK;

      case COMP_ID_NOT_FOUND:
        push_loadi(bc, kGAB_TRUE, t);
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

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (compile_expression(bc) < 0)
        return COMP_ERR;
    } else {
      push_loadi(bc, kGAB_TRUE, t);
      push_slot(bc, 1);
    }

    return COMP_OK;
  }

err:
  eat_token(bc);
  compiler_error(bc, GAB_MALFORMED_RECORD_KEY,
                 "A valid key is either:\n"
                 " | { an_identifier }\n"
                 " | { \"a string\", .a_sigil, 'or' }\n"
                 " | { [an:expression] }\n\n"
                 "A key can be followed by an " ANSI_COLOR_GREEN
                 "EQUAL" ANSI_COLOR_RESET ", and then an expression.\n"
                 "If a value is not set explicitly, it will be true.\n");
  return COMP_ERR;
}

int compile_rec_internals(struct bc *bc) {
  uint8_t size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_RBRACK))
    return size;

  int result = COMP_ERR;
  do {
    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    if (match_token(bc, TOKEN_RBRACK))
      break;

    if (compile_rec_internal_item(bc) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    size++;
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)) == COMP_OK);

  if (result == COMP_ERR)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACK) < 0)
    return COMP_ERR;

  return size;
}

int compile_record(struct bc *bc) {
  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  int size = compile_rec_internals(bc);

  if (pop_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  if (size < 0)
    return COMP_ERR;

  push_op((bc), OP_RECORD, bc->offset - 1);
  push_byte((bc), size, bc->offset - 1);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return COMP_OK;
}

int compile_record_tuple(struct bc *bc) {
  bool mv = false;
  int size = 0;

  if (match_and_eat_token(bc, TOKEN_RBRACE))
    goto fin;

  size = compile_tuple(bc, VAR_EXP, &mv);

  if (size < 0)
    return COMP_ERR;

  if (size > 255) {
    compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
    return COMP_ERR;
  }

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

fin:
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

    for (int i = n - 1; i > 0; i--) {
      push_storel((bc), local + i, t);
      push_pop((bc), 1, t);
      init_local(bc, local + i);
    }

    goto fin;
  }

  if (match_token(bc, TOKEN_LBRACE)) {
    if (compile_message(bc, val_name, t) < 0)
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

int compile_exp_lmb(struct bc *bc, bool assignable) {
  return compile_lambda(bc, 0);
}

int compile_exp_blk(struct bc *bc, bool assignable) {
  return compile_block(bc, gab_nil);
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

int compile_exp_bin(struct bc *bc, bool assignable) {
  gab_token op = prev_tok(bc);
  size_t t = bc->offset - 1;

  gab_value m;

  switch (op) {
  case TOKEN_MINUS:
    m = gab_string(gab(bc), mGAB_SUB);
    break;

  case TOKEN_PLUS:
    m = gab_string(gab(bc), mGAB_ADD);
    break;

  case TOKEN_STAR:
    m = gab_string(gab(bc), mGAB_MUL);
    break;

  case TOKEN_SLASH:
    m = gab_string(gab(bc), mGAB_DIV);
    break;

  case TOKEN_PERCENT:
    m = gab_string(gab(bc), mGAB_MOD);
    break;

  case TOKEN_PIPE:
    m = gab_string(gab(bc), mGAB_BOR);
    break;

  case TOKEN_AMPERSAND:
    m = gab_string(gab(bc), mGAB_BND);
    break;

  case TOKEN_EQUAL_EQUAL:
    m = gab_string(gab(bc), mGAB_EQ);
    break;

  case TOKEN_LESSER:
    m = gab_string(gab(bc), mGAB_LT);
    break;

  case TOKEN_LESSER_EQUAL:
    m = gab_string(gab(bc), mGAB_LTE);
    break;

  case TOKEN_LESSER_LESSER:
    m = gab_string(gab(bc), mGAB_LSH);
    break;

  case TOKEN_GREATER:
    m = gab_string(gab(bc), mGAB_GT);
    break;

  case TOKEN_GREATER_EQUAL:
    m = gab_string(gab(bc), mGAB_GTE);
    break;

  case TOKEN_GREATER_GREATER:
    m = gab_string(gab(bc), mGAB_RSH);
    break;

  default:
    assert(false && "This is an internal compiler error.");
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

  if (compile_exp_prec(bc, kUNARY) < 0)
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
    assert(false && "This is an internal compiler error.");
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
  push_loadi(bc, prev_tok(bc) == TOKEN_TRUE ? kGAB_TRUE : kGAB_FALSE,
             bc->offset - 1);
  push_slot(bc, 1);
  return COMP_OK;
}

int compile_exp_nil(struct bc *bc, bool assignable) {
  push_loadi(bc, kGAB_NIL, bc->offset - 1);
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
  case TOKEN_DOT_DOT:
    name = prev_src(bc);
    break;
  case TOKEN_LPAREN:
    name = prev_src(bc);

    if (match_and_eat_token(bc, TOKEN_RPAREN)) {
      name.len++;
      break;
    }

    if (compile_expression(bc) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RPAREN) < 0)
      return COMP_ERR;

    return compile_message(bc, gab_nil, bc->offset - 1);
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
    eat_token(bc);
    compiler_error(bc, GAB_MALFORMED_TOKEN,
                   "This is an invalid specialization definition");
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
    assert(false && "This is an internal compiler error.");
    return COMP_ERR;
  }

  return COMP_OK;
}

int compile_exp_idn(struct bc *bc, bool assignable) {
  gab_value id = prev_id(bc);

  uint8_t index = 0;
  int result = resolve_id(bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
      switch (result) {
      case COMP_ID_NOT_FOUND:
        index = compile_local(bc, id, fVAR_MUTABLE);

        // We're adding a new local, which means we retroactively need
        // to add one to each other assignment target slot
        adjust_preceding_lvalues(bc);

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
  case COMP_RESOLVED_TO_LOCAL:
    push_loadl((bc), index, bc->offset - 1);

    push_slot(bc, 1);

    return COMP_OK;

  case COMP_RESOLVED_TO_UPVALUE:
    push_loadu((bc), index, bc->offset - 1);

    push_slot(bc, 1);

    return COMP_OK;

  case COMP_ID_NOT_FOUND:
    compiler_error(bc, GAB_MISSING_IDENTIFIER, "");
    return COMP_ERR;

  default:
    return COMP_ERR;
  }
}

int compile_exp_splt(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  if (!assignable || match_ctx(bc, kTUPLE)) {
    if (compile_exp_prec(bc, kUNARY) < 0)
      return COMP_ERR;

    goto as_splat_exp;
  }

  if (!match_token(bc, TOKEN_IDENTIFIER)) {
    if (compile_exp_prec(bc, kUNARY) < 0)
      return COMP_ERR;

    goto as_splat_exp;
  }

  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  if (!match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
    if (compile_exp_startwith(bc, kUNARY, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    goto as_splat_exp;
  }

  gab_value id = prev_id(bc);

  uint8_t index;
  int result = resolve_id(bc, id, &index);

  switch (result) {
  case COMP_ID_NOT_FOUND:
    index = compile_local(bc, id, fVAR_MUTABLE);

    // We're adding a new local, which means we retroactively need
    // to add one to each other assignment target slot
    adjust_preceding_lvalues(bc);

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

as_splat_exp: {
  push_send(bc, gab_string(gab(bc), mGAB_SPLAT), 0, false, t);

  return VAR_EXP;
}
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

  push_send(bc, gab_string(gab(bc), mGAB_GET), 1, false, t);

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
  fHAS_FATARROW = 1 << 4,
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

  if (flags & fHAS_STRING || match_and_eat_token(bc, TOKEN_SIGIL) ||
      match_and_eat_token(bc, TOKEN_STRING) ||
      match_and_eat_token(bc, TOKEN_INTERPOLATION_BEGIN)) {

    if (*mv_out)
      push_trim(bc, 1, bc->offset - 1);

    if (compile_string(bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (flags & fHAS_BRACK || match_and_eat_token(bc, TOKEN_LBRACK)) {
    if (*mv_out)
      push_trim(bc, 1, bc->offset - 1);
    // record argument
    if (compile_record(bc) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (flags & fHAS_FATARROW || match_and_eat_token(bc, TOKEN_FAT_ARROW)) {
    if (*mv_out)
      push_trim(bc, 1, bc->offset - 1);

    if (compile_lambda(bc, bc->offset - 1) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  } else if (flags & fHAS_DO || match_and_eat_token(bc, TOKEN_DO)) {
    if (*mv_out)
      push_trim(bc, 1, bc->offset - 1);

    if (compile_block(bc, gab_nil) < 0)
      return COMP_ERR;

    result += 1 + *mv_out;
    *mv_out = false;
  }

  if (result > 254) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  return result;
}

int compile_exp_emp(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  gab_value val_name = trim_prev_id(bc);

  push_loadi(bc, kGAB_UNDEFINED, t);

  push_slot(bc, 1);

  bool mv;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  push_send(bc, val_name, result, mv, t);

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
  case TOKEN_DOT_DOT:
    msg = mGAB_SPLAT;
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
    compiler_error(bc, GAB_MALFORMED_MESSAGE_LITERAL,
                   "A message literal is either:\n"
                   " | &:a_normal_message\n"
                   " | &+ # or a builtin message, like " ANSI_COLOR_GREEN
                   "PLUS" ANSI_COLOR_RESET "\n"
                   "\nWhat follows the " ANSI_COLOR_GREEN
                   "AMPERSAND" ANSI_COLOR_RESET
                   " is exactly what you would type to send the message.\n");
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

  size_t t = bc->offset - 1;

  if (compile_expression(bc) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  bool mv = false;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, 1 + result + mv);

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
    }

    if (match_ctx(bc, kASSIGNMENT_TARGET)) {
      eat_token(bc);
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    }
  }

  bool mv = false;
  int result = compile_arguments(bc, &mv, 0);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_ARG_MAX) {
    compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  pop_slot(bc, result + 1 + mv);

  push_send(bc, name, result, mv, t);

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

  pop_slot(bc, result + 1 + mv);

  push_send(bc, gab_string(gab(bc), mGAB_CALL), result, mv, t);

  push_slot(bc, 1);

  return VAR_EXP;
}

int compile_exp_lmbcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_FATARROW);

  if (result < 0)
    return COMP_ERR;

  pop_slot(bc, result);

  push_send(bc, gab_string(gab(bc), mGAB_CALL), result, mv, t);

  return VAR_EXP;
}

int compile_exp_bcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_DO);

  if (result < 0)
    return COMP_ERR;

  pop_slot(bc, result);

  push_send(bc, gab_string(gab(bc), mGAB_CALL), result, mv, t);

  return VAR_EXP;
}

int compile_exp_symcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_STRING);

  pop_slot(bc, 1);

  push_send(bc, gab_string(gab(bc), mGAB_CALL), result, mv, t);

  return VAR_EXP;
}

int compile_exp_scal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_STRING);

  pop_slot(bc, 1);

  push_send(bc, gab_string(gab(bc), mGAB_CALL), result, mv, t);

  return VAR_EXP;
}

int compile_exp_rcal(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  bool mv = false;
  int result = compile_arguments(bc, &mv, fHAS_BRACK);

  pop_slot(bc, 1);

  push_send(bc, gab_string(gab(bc), mGAB_CALL), result, mv, t);

  return VAR_EXP;
}

int compile_exp_and(struct bc *bc, bool assignable) {
  uint64_t end_jump = push_jump(bc, OP_LOGICAL_AND, bc->offset - 1);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(bc, kAND) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  patch_jump(bc, end_jump);

  return COMP_OK;
}

int compile_exp_or(struct bc *bc, bool assignable) {
  uint64_t end_jump = push_jump(bc, OP_LOGICAL_OR, bc->offset - 1);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(bc, kOR) < 0)
    return COMP_ERR;

  pop_slot(bc, 1);

  patch_jump(bc, end_jump);

  return COMP_OK;
}

int compile_exp_startwith(struct bc *bc, int prec, gab_token tok) {
  struct compile_rule rule = get_rule(tok);

  if (rule.prefix == NULL) {
    s_char p = prev_src(bc);
    compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                   FMT_UNEXPECTEDTOKEN " $ cannot begin an expression.",
                   gab_string(gab(bc), "EXPRESSION"),
                   gab_nstring(gab(bc), p.len, p.data));
    return COMP_ERR;
  }

  bool assignable = prec <= kASSIGNMENT;

  int have = rule.prefix(bc, assignable);

  if (have == VAR_EXP) {
    push_trim(bc, 1, bc->offset - 1);
  }

  while (prec <= get_rule(curr_tok(bc)).prec) {
    if (have < 0)
      return COMP_ERR;

    if (eat_token(bc) < 0)
      return COMP_ERR;

    rule = get_rule(prev_tok(bc));

    if (rule.infix != NULL) {
      // Treat this as an infix expression.
      have = rule.infix(bc, assignable);

      if (have == VAR_EXP) {
        push_trim(bc, 1, bc->offset - 1);
      }
    }
  }

  if (!assignable && match_token(bc, TOKEN_EQUAL)) {
    compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  return have;
}

int compile_exp_prec(struct bc *bc, enum prec_k prec) {
  if (eat_token(bc) < 0)
    return COMP_ERR;

  return compile_exp_startwith(bc, prec, prev_tok(bc));
}

int compile_exp_brk(struct bc *bc, bool assignable) {
  size_t t = bc->offset - 1;

  int ctx = peek_ctx(bc, kLOOP, 0);

  if (ctx < 0) {
    compiler_error(bc, GAB_INVALID_BREAK,
                   ANSI_COLOR_MAGENTA
                   "break" ANSI_COLOR_RESET
                   " is only valid within the context of a " ANSI_COLOR_MAGENTA
                   "loop" ANSI_COLOR_RESET ".\n" ANSI_COLOR_MAGENTA
                   "break" ANSI_COLOR_RESET " allows " ANSI_COLOR_MAGENTA
                   "loop" ANSI_COLOR_RESET
                   " to evaluate to something other than $\n"
                   "\n | a = loop; break 'example' end"
                   "\n | a == 'example' # => true ",
                   gab_nil);
    return COMP_ERR;
  }

  if (!curr_prefix(bc)) {
    push_loadi(bc, kGAB_NIL, t);
    goto fin;
  }

  if (compile_expression(bc) < 0)
    return COMP_OK;

fin: {
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

    uint64_t jump = push_jump(bc, OP_POPJUMP_IF_TRUE, t);

    patch_loop(bc, loop, t);

    patch_jump(bc, jump);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    patch_loop(bc, loop, t);
  }

  push_loadi(bc, kGAB_NIL, t);

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
    gab_value proto = gab_sprototype(gab(bc), bc->src, f->name, f->bc.len + 4);

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

  gab_value proto = gab_sprototype(gab(bc), bc->src, f->name, f->bc.len + 4);

  uint16_t kproto = addk(bc, proto);

  push_slot(bc, 1);

  push_yield(bc, kproto, result, mv, bc->offset - 1);

  pop_slot(bc, result);

  return VAR_EXP;
}

int compile_exp_rtn(struct bc *bc, bool assignable) {
  if (!get_rule(curr_tok(bc)).prefix) {
    push_slot(bc, 1);

    push_loadi(bc, kGAB_NIL, bc->offset - 1);

    push_ret(bc, 1, false, bc->offset - 1);

    pop_slot(bc, 1);
    return COMP_OK;
  }

  bool mv;
  int result = compile_tuple(bc, VAR_EXP, &mv);

  if (result < 0)
    return COMP_ERR;

  if (result > GAB_RET_MAX) {
    compiler_error(
        bc, GAB_TOO_MANY_RETURN_VALUES,
        "For arbitrary reasons, blocks cannot return more than 255 values.");
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
    INFIX(then, AND, false),                    // THEN
    INFIX(else, OR, false),                     // ELSE
    PREFIX_INFIX(blk, bcal, SEND, false),       // DO
    NONE(),                                     // END
    PREFIX(def),                                // DEF
    PREFIX(rtn),                                // RETURN
    PREFIX(yld),                                // YIELD
    PREFIX(lop),                                // LOOP
    NONE(),                                     // UNTIL
    PREFIX(brk),                                // BREAK
    INFIX(bin, TERM, false),                    // PLUS
    PREFIX_INFIX(una, bin, TERM, false),        // MINUS
    INFIX(bin, FACTOR, false),                  // STAR
    INFIX(bin, FACTOR, false),                  // SLASH
    INFIX(bin, FACTOR, false),                  // PERCENT
    NONE(),                                     // COMMA
    INFIX(dyn, SEND, false),                    // COLON
    PREFIX_INFIX(amp, bin, BITWISE_AND, false), // AMPERSAND
    NONE(),                                     // DOLLAR
    PREFIX_INFIX(sym, symcal, SEND, false),     // SYMBOL
    PREFIX_INFIX(emp, snd, SEND, true),         // MESSAGE
    NONE(),                                     // DOT
    PREFIX(splt),                               // DOTDOT
    NONE(),                                     // EQUAL
    INFIX(bin, EQUALITY, false),                // EQUALEQUAL
    PREFIX(una),                                // QUESTION
    NONE(),                                     // BANG
    NONE(),                                     // AT
    NONE(),                                     // COLON_EQUAL
    INFIX(bin, COMPARISON, false),              // LESSER
    INFIX(bin, EQUALITY, false),                // LESSEREQUAL
    INFIX(bin, TERM, false),                    // LESSERLESSER
    INFIX(bin, COMPARISON, false),              // GREATER
    INFIX(bin, EQUALITY, false),                // GREATEREQUAL
    INFIX(bin, TERM, false),                    // GREATER_GREATER
    NONE(),                                     // ARROW
    PREFIX_INFIX(lmb, lmbcal, SEND, false),     // FATARROW
    INFIX(and, AND, false),                     // AND
    INFIX(or, OR, false),                       // OR
    PREFIX(una),                                // NOT
    PREFIX_INFIX(arr, idx, SEND, false),        // LBRACE
    NONE(),                                     // RBRACE
    PREFIX_INFIX(rec, rcal, SEND, false),       // LBRACK
    NONE(),                                     // RBRACK
    PREFIX_INFIX(grp, cal, SEND, false),        // LPAREN
    NONE(),                                     // RPAREN
    INFIX(bin, BITWISE_OR, false),              // PIPE
    PREFIX(idn),                                // ID
    PREFIX(ipm),                                // IMPLICIT
    PREFIX_INFIX(str, scal, SEND, false),       // STRING
    PREFIX_INFIX(itp, scal, SEND, false),       // INTERPOLATION END
    NONE(),                                     // INTERPOLATION MIDDLE
    NONE(),                                     // INTERPOLATION END
    PREFIX(num),                                // NUMBER
    PREFIX(bool),                               // FALSE
    PREFIX(bool),                               // TRUE
    PREFIX(nil),                                // NIL
    NONE(),                                     // NEWLINE
    NONE(),                                     // EOF
    NONE(),                                     // ERROR
};

struct compile_rule get_rule(gab_token k) { return rules[k]; }

gab_value compile(struct bc *bc, gab_value name, uint8_t narguments,
                  gab_value arguments[narguments]) {
  push_ctxframe(bc, gab_nil, false);

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;

  if (curr_tok(bc) == TOKEN_EOF)
    return gab_undefined;

  if (curr_tok(bc) == TOKEN_ERROR) {
    eat_token(bc);
    compiler_error(bc, GAB_MALFORMED_TOKEN,
                   "This token is malformed or unrecognized.");
    return gab_undefined;
  }

  for (uint8_t i = 0; i < narguments; i++)
    init_local(bc, add_local(bc, arguments[i], 0));

  if (compile_expressions_body(bc) < 0)
    return gab_undefined;

  bool mv = patch_trim(bc, VAR_EXP);

  push_ret(bc, !mv, mv, bc->offset - 1);

  gab_value p = pop_ctxframe(bc);

  if (p == gab_undefined)
    return COMP_ERR;

  if (gab(bc).flags & fGAB_DUMP_BYTECODE)
    gab_fmodinspect(stdout, GAB_VAL_TO_PROTOTYPE(p));

  gab_value main = gab_block(gab(bc), p);

  gab_iref(gab(bc), main);
  gab_egkeep(eg(bc), main);

  return main;
}

gab_value gab_cmpl(struct gab_triple gab, struct gab_cmpl_argt args) {
  gab.flags = args.flags;

  gab_value name = gab_string(gab, args.name);

  struct gab_src *src =
      gab_src(gab, name, (char *)args.source, strlen(args.source) + 1);

  struct bc bc;
  bc_create(&bc, gab, src);

  gab_value vargv[args.len];

  for (int i = 0; i < args.len; i++) {
    vargv[i] = gab_string(gab, args.argv[i]);
  }

  addk(&bc, gab_undefined);
  addk(&bc, gab_nil);
  addk(&bc, gab_false);
  addk(&bc, gab_true);

  gab_value module = compile(&bc, gab_string(gab, args.name), args.len, vargv);

  bc_destroy(&bc);

  assert(src->bytecode.len == src->bytecode_toks.len);

  return module;
}

static void compiler_error(struct bc *bc, enum gab_status e,
                           const char *note_fmt, ...) {
  if (bc->panic)
    return;

  bc->panic = true;

  va_list va;
  va_start(va, note_fmt);

  gab_fvpanic(gab(bc), stderr, va,
              (struct gab_err_argt){
                  .src = bc->src,
                  .message = gab_nil,
                  .status = e,
                  .tok = bc->offset - 1,
                  .note_fmt = note_fmt,
              });

  va_end(va);
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

  uint8_t var = have & FLAG_VAR_EXP;
  have = have >> 1;

  fprintf(stream,
          "%-25s"
          "(%d%s)\n",
          name, have, var ? " & more" : "");
  return offset + 4;
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

  uint8_t var = have & FLAG_VAR_EXP;
  have = have >> 1;

  fprintf(stream, "%-25s" ANSI_COLOR_BLUE, name);
  gab_fvalinspect(stream, msg, 0);
  fprintf(stream, ANSI_COLOR_RESET " (%d%s)\n", have, var ? " & more" : "");

  return offset + 4;
}

uint64_t dumpByteInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}

uint64_t dumpTrimInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint64_t offset) {
  uint8_t wantbyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, wantbyte);
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

uint64_t dumpPackInstruction(FILE *stream, struct gab_obj_prototype *self,
                             uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t operandA = v_uint8_t_val_at(&self->src->bytecode, offset + 2);
  uint8_t operandB = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  uint8_t have = havebyte >> 1;
  fprintf(stream, "%-25s(%hhx%s) -> %hhx %hhx\n", name, have,
          havebyte & FLAG_VAR_EXP ? " & more" : "", operandA, operandB);
  return offset + 4;
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

uint64_t dumpNConstantInstruction(FILE *stream, struct gab_obj_prototype *self,
                                  uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s", name);

  uint8_t n = v_uint8_t_val_at(&self->src->bytecode, offset + 1);

  for (int i = 0; i < n; i++) {
    uint16_t constant =
        ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 2 + (2 * i)))
            << 8 |
        v_uint8_t_val_at(&self->src->bytecode, offset + 3 + (2 * i));

    gab_fvalinspect(stdout, v_gab_value_val_at(&self->src->constants, constant),
                    0);

    if (i < n - 1)
      fprintf(stream, ", ");
  }

  fprintf(stream, "\n");
  return offset + 2 + (2 * n);
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

uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                         uint64_t offset) {
  uint8_t op = v_uint8_t_val_at(&self->src->bytecode, offset);
  switch (op) {
  case OP_SWAP:
  case OP_DUP:
  case OP_NOT:
  case OP_POP:
  case OP_TYPE:
  case OP_NEGATE:
  case OP_NOP:
    return dumpSimpleInstruction(stream, self, offset);
  case OP_PACK:
    return dumpPackInstruction(stream, self, offset);
  case OP_LOGICAL_AND:
  case OP_JUMP_IF_FALSE:
  case OP_JUMP_IF_TRUE:
  case OP_JUMP:
  case OP_POPJUMP_IF_FALSE:
  case OP_POPJUMP_IF_TRUE:
  case OP_LOGICAL_OR:
    return dumpJumpInstruction(stream, self, 1, offset);
  case OP_LOOP:
    return dumpJumpInstruction(stream, self, -1, offset);
  case OP_NCONSTANT:
    return dumpNConstantInstruction(stream, self, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(stream, self, offset);
  case OP_SEND:
  case OP_SEND_BLOCK:
  case OP_SEND_NATIVE:
  case OP_SEND_PROPERTY:
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
  case OP_DYNSEND:
    return dumpDynSendInstruction(stream, self, offset);
  case OP_POP_N:
  case OP_STORE_LOCAL:
  case OP_POPSTORE_LOCAL:
  case OP_LOAD_UPVALUE:
  case OP_INTERPOLATE:
  case OP_SHIFT:
  case OP_LOAD_LOCAL:
    return dumpByteInstruction(stream, self, offset);
  case OP_NPOPSTORE_STORE_LOCAL:
  case OP_NPOPSTORE_LOCAL:
  case OP_NLOAD_UPVALUE:
  case OP_NLOAD_LOCAL: {
    const char *name =
        gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];

    uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);

    fprintf(stream, "%-25s%hhx: ", name, operand);

    for (int i = 0; i < operand - 1; i++) {
      fprintf(stream, "%hhx, ",
              v_uint8_t_val_at(&self->src->bytecode, offset + 2 + i));
    }

    fprintf(stream, "%hhx\n",
            v_uint8_t_val_at(&self->src->bytecode, offset + 1 + operand));

    return offset + 2 + operand;
  }
  case OP_RETURN:
    return dumpReturnInstruction(stream, self, offset);
  case OP_YIELD: {
    return dumpYieldInstruction(stream, self, offset);
  }
  case OP_SPEC: {
    offset++;
    uint16_t proto_constant =
        ((((uint16_t)self->src->bytecode.data[offset]) << 8) |
         self->src->bytecode.data[offset + 1]);
    offset += 4;

    gab_value pval = v_gab_value_val_at(&self->src->constants, proto_constant);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    fprintf(stream, "%-25s" ANSI_COLOR_CYAN "%-20s\n" ANSI_COLOR_RESET,
            "OP_MESSAGE", gab_strdata(&p->name));

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
        (((uint16_t)self->src->bytecode.data[offset] << 8) |
         self->src->bytecode.data[offset + 1]);

    offset += 2;

    gab_value pval = v_gab_value_val_at(&self->src->constants, proto_constant);

    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    printf("%-25s" ANSI_COLOR_CYAN "%-20s\n" ANSI_COLOR_RESET, "OP_BLOCK",
           gab_strdata(&p->name));

    for (int j = 0; j < p->as.block.nupvalues; j++) {
      uint8_t flags = p->data[j * 2];
      uint8_t index = p->data[j * 2 + 1];
      int isLocal = flags & fVAR_LOCAL;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_TRIM_UP1:
  case OP_TRIM_UP2:
  case OP_TRIM_UP3:
  case OP_TRIM_UP4:
  case OP_TRIM_UP5:
  case OP_TRIM_UP6:
  case OP_TRIM_UP7:
  case OP_TRIM_UP8:
  case OP_TRIM_UP9:
  case OP_TRIM_DOWN1:
  case OP_TRIM_DOWN2:
  case OP_TRIM_DOWN3:
  case OP_TRIM_DOWN4:
  case OP_TRIM_DOWN5:
  case OP_TRIM_DOWN6:
  case OP_TRIM_DOWN7:
  case OP_TRIM_DOWN8:
  case OP_TRIM_DOWN9:
  case OP_TRIM_EXACTLY0:
  case OP_TRIM_EXACTLY1:
  case OP_TRIM_EXACTLY2:
  case OP_TRIM_EXACTLY3:
  case OP_TRIM_EXACTLY4:
  case OP_TRIM_EXACTLY5:
  case OP_TRIM_EXACTLY6:
  case OP_TRIM_EXACTLY7:
  case OP_TRIM_EXACTLY8:
  case OP_TRIM_EXACTLY9:
  case OP_TRIM: {
    return dumpTrimInstruction(stream, self, offset);
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
  uint64_t offset = proto->offset;

  uint64_t end = proto->offset + proto->as.block.len;

  while (offset < end) {
    fprintf(stream, ANSI_COLOR_YELLOW "%04lu " ANSI_COLOR_RESET, offset);
    offset = dumpInstruction(stream, proto, offset);
  }

  return 0;
}
