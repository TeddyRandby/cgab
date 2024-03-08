#define TOKEN_NAMES
#include "colors.h"
#include "core.h"
#include "engine.h"
#include "gab.h"
#include "lexer.h"

struct frame {
  v_uint8_t bc;
  v_uint64_t bc_toks;
  gab_value name;

  unsigned char pprev_op;

  size_t prev_op_at;
  unsigned char prev_op;

  uint16_t next_slot;
  uint16_t nslots;

  unsigned char narguments;
  unsigned char next_local;
  unsigned char nlocals;
  unsigned char nupvalues;

  char local_flags[GAB_LOCAL_MAX];
  gab_value local_names[GAB_LOCAL_MAX];

  char upv_flags[GAB_UPVALUE_MAX];
  char upv_indexes[GAB_UPVALUE_MAX];
};

// ORDER_MATTERS
enum lvalue_k {
  kNEW_LOCAL,
  kNEW_REST_LOCAL,
  kEXISTING_LOCAL,
  kEXISTING_REST_LOCAL,
  kPROP,
  kINDEX,
  kDYN_PROP,
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
  kCONTEXT_NKINDS,
};

struct context {
  enum context_k kind;

  union {
    v_lvalue assignment_target;
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

// static inline struct gab_gc *gc(struct bc *bc) { return bc->gab.gc; }
static inline struct gab_eg *eg(struct bc *bc) { return bc->gab.eg; }
// static inline struct gab_vm *vm(struct bc *bc) { return bc->gab.vm; }
static inline struct gab_triple gab(struct bc *bc) { return bc->gab; }

/*
  Precedence rules for the parsing of expressions.
*/
enum prec_k { kNONE, kASSIGNMENT, kBINARY_SEND, kUNARY_SEND, kSEND, kPRIMARY };

typedef struct mv_t {
  int status;
  bool multi;
} mv;

typedef mv (*compile_f)(struct bc *, mv lhs, bool);
struct compile_rule {
  compile_f prefix;
  compile_f infix;
  enum prec_k prec;
};

static void bc_create(struct bc *self, struct gab_triple gab,
                      struct gab_src *source) {
  memset(self, 0, sizeof(*self));

  self->src = source;
  self->gab = gab;
}

static void bc_destroy(struct bc *self) {}

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
  COMP_COULD_NOT_INLINE = -10,
  COMP_MAX = INT32_MAX,
};

#define MV_ERR ((mv){COMP_ERR, false})
#define MV_OK ((mv){COMP_OK, false})
#define MV_EMPTY ((mv){0})
#define MV_OK_WITH(n) ((mv){n, false})
#define MV_MULTI ((mv){0, true})
#define MV_MULTI_WITH(n) ((mv){n, true})

#define FMT_UNEXPECTEDTOKEN "Expected $ instead."
static int compiler_error(struct bc *bc, enum gab_status e,
                          const char *help_fmt, ...);

static bool match_token(struct bc *bc, gab_token tok);

static int eat_token(struct bc *bc);

//------------------- Token Helpers -----------------------
static size_t prev_line(struct bc *bc) {
  return v_uint64_t_val_at(&bc->src->token_lines, bc->offset - 1);
}

static gab_token curr_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset);
}

static gab_token prev_tok(struct bc *bc) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset - 1);
}

// static s_char curr_src(struct bc *bc) {
//   return v_s_char_val_at(&bc->src->token_srcs, bc->offset);
// }

static s_char prev_src(struct bc *bc) {
  return v_s_char_val_at(&bc->src->token_srcs, bc->offset - 1);
}
static inline bool match_token(struct bc *bc, gab_token tok) {
  return v_gab_token_val_at(&bc->src->tokens, bc->offset) == tok;
}

static inline bool match_terminator(struct bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_EOF);
}

static int eat_token(struct bc *bc) {
  if (match_token(bc, TOKEN_EOF))
    return compiler_error(bc, GAB_UNEXPECTED_EOF, "");

  bc->offset++;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    return compiler_error(bc, GAB_MALFORMED_TOKEN,
                          "This token is malformed or unrecognized.");
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
    return compiler_error(bc, GAB_UNEXPECTED_TOKEN, FMT_UNEXPECTEDTOKEN,
                          gab_string(gab(bc), gab_token_names[tok]));
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

  f->pprev_op = f->prev_op;

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
#endif

  push_op(bc, OP_CONSTANT, t);
  push_short(bc, k, t);
}

static inline void push_loadi(struct bc *bc, enum gab_kind k, size_t t) {
  assert(k == kGAB_UNDEFINED || k == kGAB_NIL || k == kGAB_TRUE ||
         k == kGAB_FALSE);

  push_k(bc, k - 1, t);
};

static inline void push_loadni(struct bc *bc, enum gab_kind k, int n,
                               size_t t) {
  for (int i = 0; i < n; i++)
    push_loadi(bc, k, t);
}

static inline void push_loadk(struct bc *bc, gab_value k, size_t t) {
  push_k(bc, addk(bc, k), t);
}

static inline void push_loadl(struct bc *bc, uint8_t local, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

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
#endif

  push_op(bc, OP_STORE_LOCAL, t);
  push_byte(bc, local, t);
  return;
}

static inline uint8_t encode_arity(mv v) {
  assert(v.status < 64);
  return ((uint8_t)v.status << 2) | v.multi;
}

static inline void push_ret(struct bc *bc, mv rhs, size_t t) {
  assert(rhs.status < 16);

#if cGAB_TAILCALL
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->prev_op == OP_SEND) {
    uint8_t have_byte = v_uint8_t_val_at(&f->bc, f->bc.len - 1);
    v_uint8_t_set(&f->bc, f->bc.len - 1, have_byte | fHAVE_TAIL);
    rhs.status -= !rhs.multi;
    rhs.multi = true;
  } else if (f->prev_op == OP_TRIM && f->pprev_op == OP_SEND) {
    uint8_t have_byte = v_uint8_t_val_at(&f->bc, f->bc.len - 3);
    v_uint8_t_set(&f->bc, f->bc.len - 3, have_byte | fHAVE_TAIL);
    f->prev_op = f->pprev_op;
    f->bc.len -= 2;
    f->bc_toks.len -= 2;
    rhs.status -= !rhs.multi;
    rhs.multi = true;
  }
#endif

  push_op(bc, OP_RETURN, t);
  push_byte(bc, encode_arity(rhs), t);
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

static inline void push_tuple(struct bc *bc, mv rhs, size_t t) {
  assert(rhs.status < 16);

  push_op(bc, OP_TUPLE, t);
  push_byte(bc, encode_arity(rhs), t);
}

static inline void push_nnop(struct bc *bc, uint8_t n, size_t t) {
  for (int i = 0; i < n; i++)
    push_byte(bc, OP_NOP, t); // Don't count this as an op
}

static inline bool patch_trim(struct bc *bc, uint8_t want) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  assert(ctx >= 0 && "Internal compiler error: no frame context");
  struct frame *f = &bc->contexts[ctx].as.frame;

  switch (f->prev_op) {
  case OP_TRIM:
    if (want != VAR_EXP) {
      v_uint8_t_set(&f->bc, f->bc.len - 1, want); /* want */
    } else {
      f->prev_op = f->pprev_op;
      f->bc.len -= 2;
      f->bc_toks.len -= 2;
    }
    return true;
  }

  return false;
}

static inline mv push_dynsend(struct bc *bc, mv lhs, mv rhs, size_t t) {
  assert(rhs.status < 16);

  push_op(bc, OP_DYNSEND, t);
  push_nnop(bc, 2, t);
  push_byte(bc, encode_arity(rhs), t);

  return MV_MULTI;
}

static inline mv push_send(struct bc *bc, gab_value m, mv lhs, mv rhs,
                           size_t t) {
  assert(rhs.status < 16);

  if (gab_valkind(m) == kGAB_STRING)
    m = gab_message(gab(bc), m);

  assert(gab_valkind(m) == kGAB_MESSAGE);

  uint16_t ks = addk(bc, m);
  addk(bc, gab_undefined);

  for (int i = 0; i < cGAB_SEND_CACHE_LEN; i++) {
    addk(bc, gab_undefined);
    addk(bc, gab_undefined);
    addk(bc, gab_undefined);
  }

  switch (lhs.status) {
  case 0:
    assert(lhs.multi);
    lhs.status++;
    break;
  case 1:
    rhs.status++;
    break;
  default:
    if (rhs.status == 0 && !rhs.multi) {
      rhs = lhs;
      lhs.status = 1;
      break;
    }

    return compiler_error(
               bc, GAB_TOO_MANY_ARGUMENTS,
               "A tuple is not a valid receiver of an infix message."),
           MV_ERR;
  }

  push_op(bc, OP_SEND, t);
  push_short(bc, ks, t);
  push_byte(bc, encode_arity(rhs), t);

  return MV_MULTI_WITH(lhs.status - 1);
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
#endif

  push_op(bc, OP_POP, t);
}

static inline void push_trim(struct bc *bc, uint8_t want, size_t t) {
  // int ctx = peek_ctx(bc, kFRAME, 0);
  // assert(ctx >= 0 && "Internal compiler error: no frame context");
  // struct frame *f = &bc->contexts[ctx].as.frame;

  // if (f->prev_op == OP_TRIM) {
  //   f->bc.data[f->prev_op_at + 1] = want;
  //   return;
  // }

  push_op(bc, OP_TRIM, t);
  push_byte(bc, want, t);
}

static inline void push_pack(struct bc *bc, mv rhs, uint8_t below,
                             uint8_t above, size_t t) {
  push_op(bc, OP_PACK, t);
  push_byte(bc, encode_arity(rhs), t);
  push_byte(bc, below, t);
  push_byte(bc, above, t);
}

// static gab_value curr_id(struct bc *bc) {
//   s_char s = curr_src(bc);
//
//   gab_value sv = gab_nstring(gab(bc), s.len, s.data);
//
//   return sv;
// }

static gab_value prev_id(struct bc *bc) {
  s_char s = prev_src(bc);

  return gab_nstring(gab(bc), s.len, s.data);
}

static gab_value trim_prev_id(struct bc *bc) {
  s_char s = prev_src(bc);

  s.data++;
  s.len--;

  return gab_nstring(gab(bc), s.len, s.data);
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

  printf("[PUSH] +%d -> %d |%s:%d\n", n, f->next_slot + n, file, line);

  if (f->next_slot + n >= UINT16_MAX) {
    assert(false && "Too many slots. This is an internal compiler error.");
  }

  f->next_slot += n;

  if (f->next_slot > f->nslots)
    f->nslots = f->next_slot;
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

  printf("[ POP] -%d -> %d |%s:%d\n", n, f->next_slot - n, file, line);

  if (f->next_slot - n < 0) {
    assert(false && "Too few slots. This is an internal compiler error.");
  }

  f->next_slot -= n;
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

static int init_local(struct bc *bc, int local) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;
  f->local_flags[local] |= fLOCAL_INITIALIZED;

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static int add_local(struct bc *bc, gab_value name, uint8_t flags) {
  addk(bc, name);
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  if (f->nlocals == GAB_LOCAL_MAX)
    return compiler_error(
        bc, GAB_TOO_MANY_LOCALS,
        "For arbitrary reasons, blocks cannot decalre more than 255 locals.");

  uint8_t local = f->next_local;
  f->local_names[local] = name;
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

  if (count == GAB_UPVALUE_MAX)
    return compiler_error(
        bc, GAB_TOO_MANY_UPVALUES,
        "For arbitrary reasons, blocks cannot capture more than 255 "
        "variables.");

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
      if (!(f->local_flags[local] & fLOCAL_INITIALIZED))
        return compiler_error(
            bc, GAB_REFERENCE_BEFORE_INITIALIZE,
            "The variable $ was referenced before it was initialized.", name);

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
  if (depth >= bc->ncontext)
    return COMP_UPVALUE_NOT_FOUND;

  int local = resolve_local(bc, name, depth + 1);

  if (local >= 0) {
    int ctx = peek_ctx(bc, kFRAME, depth + 1);
    struct frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->local_flags[local] |= fLOCAL_CAPTURED;

    return add_upvalue(bc, depth, local, flags | fLOCAL_LOCAL);
  }

  int upvalue = resolve_upvalue(bc, name, depth + 1);
  if (upvalue >= 0) {
    int ctx = peek_ctx(bc, kFRAME, depth + 1);
    struct frame *f = &bc->contexts[ctx].as.frame;

    uint8_t flags = f->upv_flags[upvalue];
    return add_upvalue(bc, depth, upvalue, flags & ~fLOCAL_LOCAL);
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
      *value_in = arg;

    return COMP_RESOLVED_TO_UPVALUE;
  }

  if (value_in)
    *value_in = arg;

  return COMP_RESOLVED_TO_LOCAL;
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

static struct frame *push_ctxframe(struct bc *bc, gab_value name,
                                   bool is_message) {
  int ctx = push_ctx(bc, kFRAME);

  assert(ctx >= 0 && "Failed to push frame context");

  struct context *c = bc->contexts + ctx;

  struct frame *f = &c->as.frame;

  memset(f, 0, sizeof(struct frame));

  bool is_anonymous = name == gab_nil;

  f->name = is_anonymous ? gab_string(gab(bc), "anonymous") : name;

  addk(bc, name);

  init_local(bc, add_local(bc, gab_string(gab(bc), "self"), 0));

  return f;
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
  else if (f->bc.data[4] == OP_TRIM)
    v_uint8_t_set(&f->bc, 5, nlocals);

  bc->next_block =
      gab_srcappend(bc->src, f->bc.len, f->bc.data, f->bc_toks.data);

  v_uint8_t_destroy(&f->bc);
  v_uint64_t_destroy(&f->bc_toks);

  return p;
}

struct compile_rule get_rule(gab_token k);
static mv compile_exp_startwith(struct bc *bc, int prec, gab_token rule);
static mv compile_expression_prec(struct bc *bc, enum prec_k prec);
static mv compile_expression(struct bc *bc);
static mv compile_tuple(struct bc *bc, uint8_t want);

static bool curr_prefix(struct bc *bc, enum prec_k prec) {
  struct compile_rule rule = get_rule(curr_tok(bc));
  bool has_prefix = rule.prefix != NULL;
  bool has_infix = rule.infix != NULL;
  return has_prefix && (!has_infix || rule.prec > prec);
}

static mv compile_optional_expression_prec(struct bc *bc, enum prec_k prec) {
  if (!curr_prefix(bc, prec))
    return MV_EMPTY;

  return compile_expression_prec(bc, prec);
}

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

static int encode_codepoint(char *out, int utf) {
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
static a_char *parse_raw_str(struct bc *bc, s_char raw_str) {
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

static mv compile_strlit(struct bc *bc) {
  a_char *parsed = parse_raw_str(bc, prev_src(bc));

  if (parsed == NULL) {
    compiler_error(bc, GAB_MALFORMED_STRING,
                   "Single quoted strings can contain interpolations.\n"
                   "\n | 'this is { an:interpolation }'\n"
                   "\nBoth single and double quoted strings can contain escape "
                   "sequences.\n"
                   "\n | 'a newline -> \\n, or a forward slash -> \\\\'"
                   "\n | \"arbitrary unicode: \\u[2502]\"");
    return MV_ERR;
  }

  push_loadk((bc), gab_nstring(gab(bc), parsed->len, parsed->data),
             bc->offset - 1);

  a_char_destroy(parsed);

  push_slot(bc, 1);

  return MV_OK;
}

static mv compile_sigil(struct bc *bc) {
  gab_value sym = trim_prev_id(bc);

  push_loadk(bc, sym, bc->offset - 1);

  push_slot(bc, 1);

  return MV_OK;
}

static mv compile_string(struct bc *bc) {
  if (prev_tok(bc) == TOKEN_SIGIL)
    return compile_sigil(bc);

  if (prev_tok(bc) == TOKEN_STRING)
    return compile_strlit(bc);

  if (prev_tok(bc) == TOKEN_INTERPOLATION_BEGIN) {
    int result = COMP_OK;
    uint8_t n = 0;

    do {
      if (compile_strlit(bc).status < 0)
        return MV_ERR;

      n++;

      if (match_token(bc, TOKEN_INTERPOLATION_END)) {
        goto fin;
      }

      if (compile_expression(bc).status < 0)
        return MV_ERR;

      n++;
    } while ((result = match_and_eat_token(bc, TOKEN_INTERPOLATION_MIDDLE)));

  fin:
    if (result < 0)
      return MV_ERR;

    if (expect_token(bc, TOKEN_INTERPOLATION_END) < 0)
      return MV_ERR;

    if (compile_strlit(bc).status < 0)
      return MV_ERR;
    n++;

    // Concat the final string.
    push_op((bc), OP_INTERPOLATE, bc->offset - 1);
    push_byte((bc), n, bc->offset - 1);

    pop_slot(bc, n - 1);

    return MV_OK;
  }

  return MV_ERR;
}

static int compile_local(struct bc *bc, gab_value name, uint8_t flags) {
  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  for (int local = f->next_local - 1; local >= 0; local--)
    if (name == f->local_names[local])
      return compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");

  return add_local(bc, name, flags);
}

static int compile_parameters_internal(struct bc *bc, int *below,
                                       uint8_t *narguments) {
  int backup_below;
  uint8_t backup_narguments;

  if (!below)
    below = &backup_below;

  if (!narguments)
    narguments = &backup_narguments;

  *below = -1;
  *narguments = 0;
  int result = COMP_OK;

  if (match_and_eat_token(bc, TOKEN_NEWLINE))
    goto fin;

  do {
    if (*narguments >= GAB_ARG_MAX) {
      compiler_error(bc, GAB_TOO_MANY_PARAMETERS, "");
      return COMP_ERR;
    }

    *narguments = *narguments + 1;

    switch (match_and_eat_token(bc, TOKEN_DOT_DOT)) {
    case COMP_OK:
      if (*below >= 0) {
        int ctx = peek_ctx(bc, kFRAME, 0);
        struct frame *f = &bc->contexts[ctx].as.frame;

        gab_value other_name = f->local_names[*below + 1];

        return compiler_error(
            bc, GAB_INVALID_REST_VARIABLE,
            "The parameter '$' at index $ is already a 'rest' parameter.\n"
            "\nBlocks can only declare one parameter as a 'rest' parameter.\n",
            other_name, gab_number(*below));
      }

      *narguments = *narguments - 1;
      *below = *narguments;
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

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

fin:
  return result;
}

static int compile_parameters(struct bc *bc) {
  // Somehow track below and above for packing
  int below;
  uint8_t narguments;

  if (push_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  if (compile_parameters_internal(bc, &below, &narguments) < 0)
    return COMP_ERR;

  if (pop_ctx(bc, kTUPLE) < 0)
    return COMP_ERR;

  if (below >= 0)
    push_pack(bc, MV_MULTI, below + 1, narguments - below, bc->offset - 1);

  push_trim(bc, 0, bc->offset - 1);

  return below >= 0 ? VAR_EXP : narguments;
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

static mv compile_expressions_body(struct bc *bc) {
  mv result = MV_OK;

  if (skip_newlines(bc) < 0)
    return MV_ERR;

  result = compile_expression(bc);

  if (result.status < 0)
    return MV_ERR;

  if (match_terminator(bc))
    goto fin;

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return MV_ERR;

  if (skip_newlines(bc) < 0)
    return MV_ERR;

  while (!match_terminator(bc) && !match_token(bc, TOKEN_EOF)) {
    push_pop(bc, 1, bc->offset - 1);

    pop_slot(bc, 1);

    result = compile_expression(bc);

    if (result.status < 0)
      return MV_ERR;

    if (match_terminator(bc))
      goto fin;

    if (expect_token(bc, TOKEN_NEWLINE) < 0)
      return MV_ERR;

    if (skip_newlines(bc) < 0)
      return MV_ERR;
  }

fin:
  return result;
}

static mv compile_expressions(struct bc *bc) {
  uint64_t line = prev_line(bc);

  if (compile_expressions_body(bc).status < 0)
    return MV_ERR;

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    compiler_error(bc, GAB_MISSING_END,
                   "Make sure the block at line $ is closed.",
                   gab_number(line));

    return MV_ERR;
  }

  return MV_OK;
}

static mv compile_block_body(struct bc *bc) {
  mv result = compile_expressions(bc);

  if (result.status < 0)
    return MV_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return MV_ERR;

  // bool mv = patch_trim(bc, VAR_EXP);
  push_ret(bc, result, bc->offset - 1);

  return MV_OK;
}

static mv compile_block(struct bc *bc, gab_value name) {
  struct frame *f = push_ctxframe(bc, name, false);

  int narguments = compile_parameters(bc);

  if (narguments < 0)
    return MV_ERR;

  f->narguments = narguments;

  if (compile_block_body(bc).status < 0)
    return MV_ERR;

  gab_value p = pop_ctxframe(bc);

  if (p == gab_undefined)
    return MV_ERR;

  push_block(bc, p, bc->offset - 1);

  push_slot(bc, 1);

  return MV_OK;
}

static mv compile_expression(struct bc *bc) {
  return compile_expression_prec(bc, kASSIGNMENT);
}

static mv compile_tuple_prec(struct bc *bc, enum prec_k prec, uint8_t want) {
  size_t t = bc->offset - 1;

  if (push_ctx(bc, kTUPLE) < 0)
    return MV_ERR;

  uint8_t have = 0;

  mv result = {};
  for (;;) {

    result = compile_expression_prec(bc, prec);

    if (result.status < 0)
      return MV_ERR;

    switch (match_and_eat_token(bc, TOKEN_COMMA)) {
    case COMP_OK:
      if (optional_newline(bc) < 0)
        return MV_ERR;

      if (!curr_prefix(bc, prec))
        break;

      have += result.status;

      continue;
    case COMP_TOKEN_NO_MATCH:
      break;
    default:
      return MV_ERR;
    }

    break;
  }

  result.status += have;

  if (result.status < 0)
    return MV_ERR;

  assert(match_ctx(bc, kTUPLE));
  pop_ctx(bc, kTUPLE);

  if (want == VAR_EXP)
    return result;

  if (result.status + result.multi > want)
    return compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS, ""), MV_ERR;
  else if (!result.multi && result.status < want)
    push_loadni(bc, kGAB_NIL, want - result.status, t);

  push_slot(bc, want - result.status);

  return result;
}

static inline mv compile_tuple(struct bc *bc, uint8_t want) {
  return compile_tuple_prec(bc, kASSIGNMENT, want);
}

static uint8_t rest_lvalues(v_lvalue *lvalues) {
  uint8_t rest = 0;

  for (int i = 0; i < lvalues->len; i++)
    rest += lvalues->data[i].kind == kNEW_REST_LOCAL ||
            lvalues->data[i].kind == kEXISTING_REST_LOCAL;

  return rest;
}

static void adjust_preceding_lvalues(struct bc *bc) {
  int ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

  if (ctx >= 0) {
    v_lvalue *lvalues = &bc->contexts[ctx].as.assignment_target;

    for (size_t i = 0; i < lvalues->len; i++)
      lvalues->data[i].slot++;
  }
}

static uint8_t new_lvalues(v_lvalue *lvalues) {
  uint8_t new = 0;
  for (int i = 0; i < lvalues->len; i++)
    new += lvalues->data[i].kind == kNEW_LOCAL ||
           lvalues->data[i].kind == kNEW_REST_LOCAL;
  return new;
}

static mv compile_assignment(struct bc *bc, struct lvalue target) {
  bool first = !match_ctx(bc, kASSIGNMENT_TARGET);

  if (first && push_ctx(bc, kASSIGNMENT_TARGET) < 0)
    return MV_ERR;

  int ctx = peek_ctx(bc, kASSIGNMENT_TARGET, 0);

  if (ctx < 0)
    return MV_ERR;

  v_lvalue *lvalues = &bc->contexts[ctx].as.assignment_target;

  v_lvalue_push(lvalues, target);

  if (!first)
    return MV_OK;

  uint16_t targets = 1;

  while (match_and_eat_token(bc, TOKEN_COMMA)) {
    if (compile_expression_prec(bc, kASSIGNMENT).status < 0)
      return MV_ERR;

    targets++;
  }

  if (targets > lvalues->len)
    return compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, ""), MV_ERR;

  uint8_t n_rest_values = rest_lvalues(lvalues);
  uint8_t n_new_values = new_lvalues(lvalues);

  if (n_rest_values > 1)
    return compiler_error(bc, GAB_INVALID_REST_VARIABLE,
                          "Only one rest value allowed"),
           MV_ERR;

  uint8_t want = n_rest_values ? VAR_EXP : lvalues->len;

  if (expect_token(bc, TOKEN_EQUAL) < 0)
    return MV_ERR;

  size_t t = bc->offset - 1;

  mv rhs = compile_tuple(bc, want);

  // In the scenario where we want ALL possible values,
  // compile_tuple will only push one slot. Here we know
  // A minimum number of slots that we will have bc we call pack
  if (n_rest_values && rhs.status < n_new_values)
    push_slot(bc, n_new_values - rhs.status);

  if (rhs.status < 0)
    return MV_ERR;

  if (n_rest_values) {
    for (uint8_t i = 0; i < lvalues->len; i++) {
      if (lvalues->data[i].kind == kEXISTING_REST_LOCAL ||
          lvalues->data[i].kind == kNEW_REST_LOCAL) {
        uint8_t before = i;
        uint8_t after = lvalues->len - i - 1;

        push_pack(bc, rhs, before, after, t);

        int slots = lvalues->len - rhs.status;

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

    case kDYN_PROP:
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

    case kDYN_PROP: {
      push_dynsend(bc, MV_OK, MV_OK, t);

      if (!is_last_assignment)
        push_trim(bc, 0, t);

      pop_slot(bc, 2 + !is_last_assignment);
      break;
    }

    case kPROP: {
      push_send(bc, lval.as.property, MV_OK, MV_OK, t);

      if (!is_last_assignment)
        push_trim(bc, 0, t);

      pop_slot(bc, 1 + !is_last_assignment);
      break;
    }

    case kINDEX: {
      push_send(bc, lval.as.property, MV_OK, MV_OK_WITH(2), t);

      if (!is_last_assignment)
        push_trim(bc, 0, t);

      pop_slot(bc, 2 + !is_last_assignment);
      break;
    }
    }
  }

  v_lvalue_destroy(lvalues);

  assert(match_ctx(bc, kASSIGNMENT_TARGET));
  pop_ctx(bc, kASSIGNMENT_TARGET);

  return MV_OK;
}

static mv compile_lvalue(struct bc *bc, bool assignable, gab_value name,
                         int new_flags) {
  uint8_t index = 0;
  int result = resolve_id(bc, name, &index);

  uint8_t adjust = new_flags & fLOCAL_REST ? true : false;

  switch (result) {
  case COMP_ID_NOT_FOUND:
    index = compile_local(bc, name, new_flags);

    // We're adding a new local, which means we retroactively need
    // to add one to each other assignment target slot
    adjust_preceding_lvalues(bc);

    return compile_assignment(bc, (struct lvalue){
                                      .kind = kNEW_LOCAL + adjust,
                                      .slot = peek_slot(bc),
                                      .as.local = index,
                                  });

  case COMP_RESOLVED_TO_LOCAL:
    return compile_assignment(bc, (struct lvalue){
                                      .kind = kEXISTING_LOCAL + adjust,
                                      .slot = peek_slot(bc),
                                      .as.local = index,
                                  });

  case COMP_RESOLVED_TO_UPVALUE:
    return compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                          "Captured variables are not assignable."),
           MV_ERR;
  default:
    return MV_ERR;
  }
}

static int compile_rec_internal_item(struct bc *bc) {
  if (match_and_eat_token(bc, TOKEN_SIGIL) ||
      match_and_eat_token(bc, TOKEN_STRING) ||
      match_and_eat_token(bc, TOKEN_INTERPOLATION_BEGIN)) {
    size_t t = bc->offset - 1;

    if (compile_string(bc).status < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (compile_expression(bc).status < 0)
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
      if (compile_expression(bc).status < 0)
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

    if (compile_expression(bc).status < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    if (match_and_eat_token(bc, TOKEN_EQUAL)) {
      if (compile_expression(bc).status < 0)
        return COMP_ERR;
    } else {
      push_loadi(bc, kGAB_TRUE, t);
      push_slot(bc, 1);
    }

    return COMP_OK;
  }

err:
  eat_token(bc);
  return compiler_error(bc, GAB_MALFORMED_RECORD_KEY,
                        "A valid key is either:\n"
                        " | { an_identifier }\n"
                        " | { \"a string\", .a_sigil, 'or' }\n"
                        " | { [an:expression] }\n\n"
                        "A key can be followed by an " ANSI_COLOR_GREEN
                        "EQUAL" ANSI_COLOR_RESET ", and then an expression.\n"
                        "If a value is not set explicitly, it will be true.\n");
}

static int compile_rec_internals(struct bc *bc) {
  uint8_t size = 0;

  if (match_and_eat_token(bc, TOKEN_RBRACK))
    return size;

  int result = COMP_ERR;
  do {
    if (match_token(bc, TOKEN_RBRACK))
      break;

    if (compile_rec_internal_item(bc) < 0)
      return COMP_ERR;

    if (size > GAB_ARG_MAX)
      return compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");

    size++;
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)) == COMP_OK);

  if (result == COMP_ERR)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACK) < 0)
    return COMP_ERR;

  return size;
}

static mv compile_record(struct bc *bc) {
  if (push_ctx(bc, kTUPLE) < 0)
    return MV_ERR;

  int size = compile_rec_internals(bc);

  if (pop_ctx(bc, kTUPLE) < 0)
    return MV_ERR;

  if (size < 0)
    return MV_ERR;

  push_op((bc), OP_RECORD, bc->offset - 1);
  push_byte((bc), size, bc->offset - 1);

  pop_slot(bc, size * 2);

  push_slot(bc, 1);

  return MV_OK;
}

static mv compile_record_tuple(struct bc *bc) {
  mv rhs = {0};

  if (match_and_eat_token(bc, TOKEN_RBRACE))
    goto fin;

  rhs = compile_tuple(bc, VAR_EXP);

  if (rhs.status < 0)
    return MV_ERR;

  if (rhs.status > GAB_ARG_MAX)
    return compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, ""),
           MV_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return MV_ERR;

fin:
  push_tuple((bc), rhs, bc->offset - 1);

  pop_slot(bc, rhs.status);

  push_slot(bc, 1);

  return MV_OK;
}

enum {
  fHAS_PAREN = 1 << 0,
  fHAS_BRACK = 1 << 1,
  fHAS_DO = 1 << 2,
  fHAS_STRING = 1 << 3,
};

mv compile_send(struct bc *bc, mv lhs, bool assignable) {
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    size_t t = bc->offset - 1;

    if (compile_expression(bc).status < 0)
      return MV_ERR;

    if (expect_token(bc, TOKEN_RBRACK) < 0)
      return MV_ERR;

    if (assignable && !match_ctx(bc, kTUPLE))
      if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL))
        return compile_assignment(bc,
                                  (struct lvalue){
                                      .kind = kDYN_PROP,
                                      .slot = peek_slot(bc),
                                  }),
               MV_ERR;

    mv rhs = compile_optional_expression_prec(bc, kSEND + 1);

    if (rhs.status < 0)
      return MV_ERR;

    if (rhs.status > GAB_ARG_MAX)
      return compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, ""), MV_ERR;

    pop_slot(bc, rhs.status + rhs.multi);

    return push_dynsend(bc, lhs, rhs, t);
  }

  if (optional_newline(bc) < 0)
    return MV_ERR;

  if (!match_and_eat_tokoneof(bc, TOKEN_IDENTIFIER, TOKEN_OPERATOR))
    return compiler_error(bc, GAB_MALFORMED_MESSAGE, ""), MV_ERR;

  size_t t = bc->offset - 1;

  gab_value name = prev_id(bc);

  mv rhs = compile_optional_expression_prec(bc, kSEND + 1);

  if (assignable && !match_ctx(bc, kTUPLE)) {
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL))
      return compile_assignment(bc, (struct lvalue){
                                        .kind = rhs.status ? kINDEX : kPROP,
                                        .slot = peek_slot(bc),
                                        .as.property = name,
                                    });

    if (match_ctx(bc, kASSIGNMENT_TARGET)) {
      eat_token(bc);
      compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    }
  }

  if (rhs.status < 0)
    return MV_ERR;

  if (rhs.status > GAB_ARG_MAX)
    return compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, ""), MV_ERR;

  pop_slot(bc, rhs.status + rhs.multi);

  return push_send(bc, name, lhs, rhs, t);
}

//---------------- Compiling Expressions ------------------

static mv compile_exp_blk(struct bc *bc, mv, bool) {
  return compile_block(bc, gab_nil);
}

static mv compile_exp_bin(struct bc *bc, mv lhs, bool assignable) {
  size_t t = bc->offset - 1;

  gab_value m = prev_id(bc);

  mv rhs = compile_optional_expression_prec(bc, kBINARY_SEND);

  if (rhs.status < 0)
    return MV_ERR;

  pop_slot(bc, rhs.status);

  return push_send(bc, m, lhs, rhs, t);
}

static mv compile_exp_str(struct bc *bc, mv, bool) {
  return compile_string(bc);
}

static mv compile_exp_rec(struct bc *bc, mv, bool) {
  return compile_record(bc);
}

static mv compile_exp_arr(struct bc *bc, mv, bool) {
  return compile_record_tuple(bc);
}

static mv compile_exp_itp(struct bc *bc, mv, bool) {
  return compile_string(bc);
}

static mv compile_exp_tup(struct bc *bc, mv, bool) {
  if (optional_newline(bc) < 0)
    return MV_ERR;

  if (match_and_eat_token(bc, TOKEN_RPAREN))
    return push_loadi(bc, kGAB_NIL, bc->offset - 1), MV_OK;

  mv result = compile_tuple(bc, VAR_EXP);

  if (result.status < 0)
    return MV_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return MV_ERR;

  return result;
}

static mv compile_exp_num(struct bc *bc, mv, bool) {
  double num = strtod((char *)prev_src(bc).data, NULL);
  push_loadk((bc), gab_number(num), bc->offset - 1);
  push_slot(bc, 1);
  return MV_OK;
}

static mv compile_exp_bool(struct bc *bc, mv, bool) {
  push_loadi(bc, prev_tok(bc) == TOKEN_TRUE ? kGAB_TRUE : kGAB_FALSE,
             bc->offset - 1);
  push_slot(bc, 1);
  return MV_OK;
}

static mv compile_exp_nil(struct bc *bc, mv, bool) {
  push_loadi(bc, kGAB_NIL, bc->offset - 1);
  push_slot(bc, 1);
  return MV_OK;
}

static mv compile_exp_idn(struct bc *bc, mv lhs, bool assignable) {
  gab_value id = prev_id(bc);

  uint8_t index = 0;
  int result = resolve_id(bc, id, &index);

  if (assignable && !match_ctx(bc, kTUPLE))
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL))
      return compile_lvalue(bc, assignable, id, 0);

  switch (result) {
  case COMP_RESOLVED_TO_LOCAL:
    push_loadl((bc), index, bc->offset - 1);

    push_slot(bc, 1);

    return MV_OK;

  case COMP_RESOLVED_TO_UPVALUE:
    push_loadu((bc), index, bc->offset - 1);

    push_slot(bc, 1);

    return MV_OK;

  case COMP_ID_NOT_FOUND:
    return compiler_error(bc, GAB_MISSING_IDENTIFIER,
                          "'$' is not within lexical scope.", id),
           MV_ERR;

  default:
    return MV_ERR;
  }
}

static mv compile_exp_splt(struct bc *bc, mv, bool assignable) {
  size_t t = bc->offset - 1;

  if (!assignable || match_ctx(bc, kTUPLE)) {
    if (compile_expression_prec(bc, kUNARY_SEND).status < 0)
      return MV_ERR;

    goto as_splat_exp;
  }

  if (!match_token(bc, TOKEN_IDENTIFIER)) {
    if (compile_expression_prec(bc, kUNARY_SEND).status < 0)
      return MV_ERR;

    goto as_splat_exp;
  }

  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return MV_ERR;

  if (!match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL)) {
    if (compile_exp_startwith(bc, kUNARY_SEND, TOKEN_IDENTIFIER).status < 0)
      return MV_ERR;

    goto as_splat_exp;
  }

  return compile_lvalue(bc, assignable, prev_id(bc), fLOCAL_REST);

as_splat_exp:
  return push_send(bc, gab_string(gab(bc), mGAB_SPLAT), MV_OK, MV_OK_WITH(0),
                   t);
}

static mv compile_exp_idx(struct bc *bc, mv, bool assignable) {
  size_t t = bc->offset - 1;

  if (compile_expression(bc).status < 0)
    return MV_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return MV_ERR;

  if (assignable && !match_ctx(bc, kTUPLE))
    if (match_tokoneof(bc, TOKEN_COMMA, TOKEN_EQUAL))
      return compile_assignment(bc, (struct lvalue){
                                        .kind = kINDEX,
                                        .slot = peek_slot(bc),
                                    });

  pop_slot(bc, 2);
  push_slot(bc, 1);

  return push_send(bc, gab_string(gab(bc), mGAB_GET), MV_OK, MV_OK, t);
}

static mv compile_exp_msg_lit(struct bc *bc, mv, bool) {
  if (!match_and_eat_tokoneof(bc, TOKEN_OPERATOR, TOKEN_IDENTIFIER))
    return compiler_error(
               bc, GAB_MALFORMED_MESSAGE,
               "A message literal is either:\n"
               " | \\a_normal_message\n"
               " | \\+ # or a builtin message, like " ANSI_COLOR_GREEN
               "PLUS" ANSI_COLOR_RESET "\n"
               "Think of this syntax as 'escaping' the message itself.\n"),
           MV_ERR;

  gab_value m = prev_id(bc);

  push_loadk(bc, gab_message(gab(bc), m), bc->offset - 1);

  push_slot(bc, 1);

  return MV_OK;
}

static mv compile_exp_send(struct bc *bc, mv lhs, bool assignable) {
  return compile_send(bc, lhs, assignable);
}

static mv compile_exp_startwith(struct bc *bc, int prec, gab_token tok) {
  struct compile_rule rule = get_rule(tok);

  if (rule.prefix == NULL) {
    s_char p = prev_src(bc);
    return compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                          FMT_UNEXPECTEDTOKEN " $ cannot begin an expression.",
                          gab_string(gab(bc), "EXPRESSION"),
                          gab_nstring(gab(bc), p.len, p.data)),
           MV_ERR;
  }

  bool assignable = prec <= kASSIGNMENT;

  mv have = rule.prefix(bc, MV_ERR, assignable);

  while (prec <= get_rule(curr_tok(bc)).prec) {
    if (have.status < 0)
      return MV_ERR;

    if (eat_token(bc) < 0)
      return MV_ERR;

    rule = get_rule(prev_tok(bc));

    if (rule.infix != NULL) {
      if (have.multi) {
        push_trim(bc, 1, bc->offset - 1);
        have.status += 1;
        have.multi = false;
      }

      have = rule.infix(bc, have, assignable);
    }
  }

  // if (!assignable && match_token(bc, TOKEN_EQUAL))
  //   return compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, ""), MV_ERR;

  return have;
}

static mv compile_expression_prec(struct bc *bc, enum prec_k prec) {
  if (eat_token(bc) < 0)
    return MV_ERR;

  return compile_exp_startwith(bc, prec, prev_tok(bc));
}

// All of the expression compiling functions follow the naming convention
// compile_exp_<name>.
#define NONE()                                                                 \
  { NULL, NULL, kNONE }
#define PREFIX(fnc)                                                            \
  { compile_exp_##fnc, NULL, kNONE }
#define INFIX(fnc, prec)                                                       \
  { NULL, compile_exp_##fnc, k##prec }

// ----------------Pratt Parsing Table ----------------------
const struct compile_rule rules[] = {
    PREFIX(blk),             // DO
    NONE(),                  // END
    NONE(),                  // COMMA
    PREFIX(splt),            // DOT_DOT
    INFIX(send, SEND),       // COLON
    PREFIX(msg_lit),         // BACKSLASH
    NONE(),                  // EQUAL
    PREFIX(arr),             // LBRACE
    NONE(),                  // RBRACE
    PREFIX(rec),             // LBRACK
    NONE(),                  // RBRACK
    PREFIX(tup),             // LPAREN
    NONE(),                  // RPAREN
    PREFIX(idn),             // ID
    INFIX(bin, BINARY_SEND), // OPERATOR
    PREFIX(str),             // SIGIL
    PREFIX(str),             // STRING
    PREFIX(itp),             // INTERPOLATION END
    NONE(),                  // INTERPOLATION MIDDLE
    NONE(),                  // INTERPOLATION END
    PREFIX(num),             // NUMBER
    PREFIX(bool),            // FALSE
    PREFIX(bool),            // TRUE
    PREFIX(nil),             // NIL
    NONE(),                  // NEWLINE
    NONE(),                  // EOF
    NONE(),                  // ERROR
};

struct compile_rule get_rule(gab_token k) { return rules[k]; }

gab_value compile(struct bc *bc, gab_value name, uint8_t narguments,
                  gab_value arguments[narguments]) {
  push_ctxframe(bc, gab_nil, false);

  push_trim(bc, narguments, bc->offset - 1);

  int ctx = peek_ctx(bc, kFRAME, 0);
  struct frame *f = &bc->contexts[ctx].as.frame;

  f->narguments = narguments;

  if (curr_tok(bc) == TOKEN_EOF)
    return gab_undefined;

  if (curr_tok(bc) == TOKEN_ERROR) {
    eat_token(bc);
    return compiler_error(bc, GAB_MALFORMED_TOKEN,
                          "This token is malformed or unrecognized.");
  }

  for (uint8_t i = 0; i < narguments; i++)
    init_local(bc, add_local(bc, arguments[i], 0));

  mv result = compile_expressions_body(bc);

  if (result.status < 0)
    return gab_undefined;

  push_ret(bc, result, bc->offset - 1);

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

static int compiler_error(struct bc *bc, enum gab_status e,
                          const char *note_fmt, ...) {
  if (bc->panic)
    return COMP_ERR;

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

  return COMP_ERR;
}

static uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                                uint64_t offset);

static uint64_t dumpSimpleInstruction(FILE *stream,
                                      struct gab_obj_prototype *self,
                                      uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s\n", name);
  return offset + 1;
}

static uint64_t dumpSendInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];

  uint16_t constant =
      ((uint16_t)v_uint8_t_val_at(&self->src->bytecode, offset + 1)) << 8 |
      v_uint8_t_val_at(&self->src->bytecode, offset + 2);

  gab_value msg = v_gab_value_val_at(&self->src->constants, constant);

  uint8_t have = v_uint8_t_val_at(&self->src->bytecode, offset + 3);

  uint8_t var = have & fHAVE_VAR;
  uint8_t tail = have & fHAVE_TAIL;
  have = have >> 2;

  fprintf(stream, "%-25s" ANSI_COLOR_BLUE, name);
  gab_fvalinspect(stream, msg, 0);
  fprintf(stream, ANSI_COLOR_RESET " (%d%s)%s\n", have, var ? " & more" : "",
          tail ? " [TAILCALL]" : "");

  return offset + 4;
}

static uint64_t dumpByteInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}

static uint64_t dumpTrimInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  uint8_t wantbyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, wantbyte);
  return offset + 2;
}

static uint64_t dumpReturnInstruction(FILE *stream,
                                      struct gab_obj_prototype *self,
                                      uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s%hhx%s\n", "RETURN", have,
          havebyte & fHAVE_VAR ? " & more" : "");
  return offset + 2;
}

static uint64_t dumpYieldInstruction(FILE *stream,
                                     struct gab_obj_prototype *self,
                                     uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s%hhx%s\n", "YIELD", have,
          havebyte & fHAVE_VAR ? " & more" : "");
  return offset + 4;
}

static uint64_t dumpPackInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
                                    uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  uint8_t operandA = v_uint8_t_val_at(&self->src->bytecode, offset + 2);
  uint8_t operandB = v_uint8_t_val_at(&self->src->bytecode, offset + 3);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->src->bytecode, offset)];
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s(%hhx%s) -> %hhx %hhx\n", name, have,
          havebyte & fHAVE_VAR ? " & more" : "", operandA, operandB);
  return offset + 4;
}

static uint64_t dumpDictInstruction(FILE *stream,
                                    struct gab_obj_prototype *self, uint8_t i,
                                    uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->src->bytecode, offset + 1);
  const char *name = gab_opcode_names[i];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
};

static uint64_t dumpConstantInstruction(FILE *stream,
                                        struct gab_obj_prototype *self,
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

static uint64_t dumpNConstantInstruction(FILE *stream,
                                         struct gab_obj_prototype *self,
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

static uint64_t dumpJumpInstruction(FILE *stream,
                                    struct gab_obj_prototype *self,
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

static uint64_t dumpInstruction(FILE *stream, struct gab_obj_prototype *self,
                                uint64_t offset) {
  uint8_t op = v_uint8_t_val_at(&self->src->bytecode, offset);
  switch (op) {
  case OP_POP:
  case OP_NOP:
    return dumpSimpleInstruction(stream, self, offset);
  case OP_PACK:
    return dumpPackInstruction(stream, self, offset);
  case OP_LOGICAL_AND:
  case OP_LOGICAL_OR:
    return dumpJumpInstruction(stream, self, 1, offset);
  case OP_NCONSTANT:
    return dumpNConstantInstruction(stream, self, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(stream, self, offset);
  case OP_SEND:
  case OP_SEND_BLOCK:
  case OP_SEND_NATIVE:
  case OP_SEND_PROPERTY:
  case OP_SEND_PRIMITIVE_CONCAT:
  case OP_SEND_PRIMITIVE_SPLAT:
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
  case OP_TAILSEND_BLOCK:
  case OP_TAILSEND_PRIMITIVE_CALL_BLOCK:
  case OP_TAILSEND_PRIMITIVE_CALL_SUSPENSE:
  case OP_LOCALSEND_BLOCK:
  case OP_LOCALTAILSEND_BLOCK:
  case OP_MATCHSEND_BLOCK:
  case OP_MATCHTAILSEND_BLOCK:
  case OP_DYNSEND:
    return dumpSendInstruction(stream, self, offset);
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
      int isLocal = p->data[j] & fLOCAL_LOCAL;
      uint8_t index = p->data[j] >> 1;
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
