#include "core.h"
#include "gab.h"
#include <stdint.h>

#define GAB_STATUS_NAMES_IMPL
#include "engine.h"

#include "colors.h"
#include "lexer.h"

#include <stdarg.h>
#include <string.h>

#define OP_HANDLER_ARGS                                                        \
  struct gab_triple __gab, uint8_t *__ip, gab_value *__kb, gab_value *__fb,    \
      gab_value *__sp

typedef a_gab_value *(*handler)(OP_HANDLER_ARGS);

// Forward declare all our opcode handlers
#define OP_CODE(name) a_gab_value *OP_##name##_HANDLER(OP_HANDLER_ARGS);
#include "bytecode.h"
#undef OP_CODE

// Plop them all in an array
static handler handlers[] = {
#define OP_CODE(name) OP_##name##_HANDLER,
#include "bytecode.h"
#undef OP_CODE
};

#if cGAB_LOG_VM
#define LOG(op) printf("OP_%s\n", gab_opcode_names[op]);
#else
#define LOG(op)
#endif

#define ATTRIBUTES [[gnu::hot, gnu::sysv_abi, gnu::flatten]]

#define CASE_CODE(name)                                                        \
  ATTRIBUTES a_gab_value *OP_##name##_HANDLER(OP_HANDLER_ARGS)

#define DISPATCH_ARGS() GAB(), IP(), KB(), FB(), SP()

#define DISPATCH(op)                                                           \
  ({                                                                           \
    uint8_t o = (op);                                                          \
    LOG(o)                                                                     \
    return handlers[o](DISPATCH_ARGS());                                       \
  })

#define NEXT() DISPATCH(*IP()++);

#define ERROR(status, help, ...)                                               \
  ({ return vm_error(GAB(), status, help __VA_OPT__(, ) __VA_ARGS__); })

/*
  Lots of helper macros.
*/
#define GAB() (__gab)
#define EG() (GAB().eg)
#define GC() (GAB().gc)
#define VM() (GAB().vm)
#define SET_BLOCK(b) (FB()[-3] = (uintptr_t)(b));
#define BLOCK() ((struct gab_obj_block *)(uintptr_t)FB()[-3])
#define BLOCK_PROTO() (GAB_VAL_TO_PROTOTYPE(BLOCK()->p))
#define IP() (__ip)
#define SP() (__sp)
#define VAR() (*SP())
#define FB() (__fb)
#define KB() (__kb)
#define LOCAL(i) (FB()[i])
#define UPVALUE(i) (BLOCK()->upvalues[i])

#if cGAB_DEBUG_VM
#define PUSH(value)                                                            \
  ({                                                                           \
    if (SP() > (FB() + BLOCK_PROTO()->nslots + 1)) {                           \
      fprintf(stderr,                                                          \
              "Stack exceeded frame "                                          \
              "(%d). %lu passed\n",                                            \
              BLOCK_PROTO()->nslots, SP() - FB() - BLOCK_PROTO()->nslots);     \
      gab_fvminspect(stdout, VM(), 0);                                         \
      exit(1);                                                                 \
    }                                                                          \
    *SP()++ = value;                                                           \
  })

#else
#define PUSH(value) (*SP()++ = value)
#endif
#define POP() (*(--SP()))
#define DROP() (SP()--)
#define POP_N(n) (SP() -= (n))
#define DROP_N(n) (SP() -= (n))
#define PEEK() (*(SP() - 1))
#define PEEK2() (*(SP() - 2))
#define PEEK3() (*(SP() - 3))
#define PEEK_N(n) (*(SP() - (n)))

#define WRITE_BYTE(dist, n) (*(IP() - dist) = (n))

#define WRITE_INLINEBYTE(n) (*IP()++ = (n))

#define SKIP_BYTE (IP()++)
#define SKIP_SHORT (IP() += 2)

#define READ_BYTE (*IP()++)
#define READ_SHORT (IP() += 2, (((uint16_t)IP()[-2] << 8) | IP()[-1]))
#define PREVIEW_SHORT (((uint16_t)IP()[0] << 8) | IP()[1])

#define READ_CONSTANT (KB()[READ_SHORT])
#define READ_CONSTANTS (KB() + READ_SHORT)

#define MISS_CACHED_SEND()                                                     \
  ({                                                                           \
    IP() -= SEND_CACHE_DIST - 1;                                               \
    return OP_SEND_HANDLER(DISPATCH_ARGS());                                   \
  })

#define MISS_CACHED_TRIM()                                                     \
  ({                                                                           \
    IP()--;                                                                    \
    return OP_TRIM_HANDLER(DISPATCH_ARGS());                                   \
  })

#define IMPL_SEND_UNARY_NUMERIC(CODE, value_type, operation_type, operation)   \
  CASE_CODE(SEND_##CODE) {                                                     \
    gab_value *ks = READ_CONSTANTS;                                            \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    SEND_GUARD_CACHED_RECEIVER_TYPE(PEEK_N(have));                             \
    ERROR_GUARD_ISN(PEEK_N(have));                                             \
                                                                               \
    operation_type val = gab_valton(PEEK_N(have));                             \
                                                                               \
    DROP_N(have);                                                              \
    PUSH(value_type(operation val));                                           \
                                                                               \
    SET_VAR(1);                                                                \
                                                                               \
    NEXT();                                                                    \
  }

#define IMPL_SEND_BINARY_NUMERIC(CODE, value_type, operation_type, operation)  \
  CASE_CODE(SEND_##CODE) {                                                     \
    gab_value *ks = READ_CONSTANTS;                                            \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    SEND_GUARD_CACHED_RECEIVER_TYPE(PEEK_N(have));                             \
                                                                               \
    if (__gab_unlikely(have < 2))                                              \
      PUSH(gab_nil), have++;                                                   \
                                                                               \
    ERROR_GUARD_ISN(PEEK_N(have));                                             \
    ERROR_GUARD_ISN(PEEK_N(have - 1));                                         \
                                                                               \
    operation_type val_b = gab_valton(PEEK_N(have - 1));                       \
    operation_type val_a = gab_valton(PEEK_N(have));                           \
                                                                               \
    DROP_N(have);                                                              \
    PUSH(value_type(val_a operation val_b));                                   \
                                                                               \
    SET_VAR(1);                                                                \
                                                                               \
    NEXT();                                                                    \
  }

// FIXME: This doesn't work
// These boolean sends don't work because there is no longer a boolean type.
// There are just sigils
#define IMPL_SEND_UNARY_BOOLEAN(CODE, value_type, operation_type, operation)   \
  CASE_CODE(SEND_##CODE) {                                                     \
    gab_value *ks = READ_CONSTANTS;                                            \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    SEND_GUARD_CACHED_RECEIVER_TYPE(PEEK_N(have));                             \
                                                                               \
    ERROR_GUARD_ISB(PEEK_N(have));                                             \
                                                                               \
    operation_type val = gab_valintob(PEEK_N(have));                           \
                                                                               \
    PUSH(value_type(operation val));                                           \
                                                                               \
    SET_VAR(1);                                                                \
                                                                               \
    NEXT();                                                                    \
  }

#define IMPL_SEND_BINARY_BOOLEAN(CODE, value_type, operation_type, operation)  \
  CASE_CODE(SEND_##CODE) {                                                     \
    gab_value *ks = READ_CONSTANTS;                                            \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    SEND_GUARD_CACHED_RECEIVER_TYPE(PEEK_N(have));                             \
                                                                               \
    if (__gab_unlikely(have < 2))                                              \
      PUSH(gab_nil), have++;                                                   \
                                                                               \
    ERROR_GUARD_ISB(PEEK_N(have));                                             \
    ERROR_GUARD_ISB(PEEK_N(have - 1));                                         \
                                                                               \
    operation_type val_b = gab_valintob(POP());                                \
    operation_type val_a = gab_valintob(POP());                                \
                                                                               \
    PUSH(value_type(val_a operation val_b));                                   \
                                                                               \
    SET_VAR(1);                                                                \
                                                                               \
    NEXT();                                                                    \
  }

#define TRIM_N(n)                                                              \
  CASE_CODE(TRIM_DOWN##n) {                                                    \
    uint8_t want = READ_BYTE;                                                  \
                                                                               \
    if (__gab_unlikely((VAR() - n) != want))                                   \
      MISS_CACHED_TRIM();                                                      \
                                                                               \
    DROP_N(n);                                                                 \
                                                                               \
    NEXT();                                                                    \
  }                                                                            \
  CASE_CODE(TRIM_EXACTLY##n) {                                                 \
    SKIP_BYTE;                                                                 \
                                                                               \
    if (__gab_unlikely(VAR() != n))                                            \
      MISS_CACHED_TRIM();                                                      \
                                                                               \
    NEXT();                                                                    \
  }                                                                            \
  CASE_CODE(TRIM_UP##n) {                                                      \
    uint8_t want = READ_BYTE;                                                  \
                                                                               \
    if (__gab_unlikely((VAR() + n) != want))                                   \
      MISS_CACHED_TRIM();                                                      \
                                                                               \
    for (int i = 0; i < n; i++)                                                \
      PUSH(gab_nil);                                                           \
                                                                               \
    NEXT();                                                                    \
  }

#define STORE_SP() (VM()->sp = SP())
#define STORE_FP() (VM()->fp = FB())
#define STORE_IP() (VM()->ip = IP())

#define STORE()                                                                \
  ({                                                                           \
    STORE_SP();                                                                \
    STORE_FP();                                                                \
    STORE_IP();                                                                \
  })

#define PUSH_FRAME(b, have)                                                    \
  ({                                                                           \
    memmove(SP() - have + 3, SP() - have, have * sizeof(gab_value));           \
    SP() += 3;                                                                 \
    SP()[-have - 1] = (uintptr_t)FB();                                         \
    SP()[-have - 2] = (uintptr_t)IP();                                         \
    SP()[-have - 3] = (uintptr_t)b;                                            \
  })

#define PUSH_ERROR_FRAME(have) ({})

#define STORE_PRIMITIVE_ERROR_FRAME(have)                                      \
  ({                                                                           \
    STORE_FP();                                                                \
    STORE_SP();                                                                \
    STORE_IP();                                                                \
    PUSH_ERROR_FRAME(have);                                                    \
  })

#define RETURN_FB() ((gab_value *)(void *)FB()[-1])
#define RETURN_IP() ((uint8_t *)(void *)FB()[-2])

#define LOAD_FRAME()                                                           \
  ({                                                                           \
    IP() = RETURN_IP();                                                        \
    FB() = RETURN_FB();                                                        \
    KB() = BLOCK_PROTO()->src->constants.data;                                 \
  })

#define SEND_CACHE_DIST 4

static inline gab_value *frame_parent(gab_value *f) { return (void *)f[-1]; }

static inline struct gab_obj_block *frame_block(gab_value *f) {
  return (void *)f[-3];
}

static inline uint8_t *frame_ip(gab_value *f) { return (void *)f[-2]; }

static inline size_t compute_token_from_ip(struct gab_obj_block *b,
                                           uint8_t *ip) {
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

  size_t offset = ip - p->src->bytecode.data - 1;

  size_t token = v_uint64_t_val_at(&p->src->bytecode_toks, offset);

  return token;
}

static inline gab_value compute_message_from_ip(struct gab_obj_block *b,
                                                uint8_t *ip) {
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

  uint16_t k = ((uint16_t)ip[-3] << 8) | ip[-2];

  gab_value m = v_gab_value_val_at(&p->src->constants, k);

  return m;
}

struct gab_err_argt vm_frame_build_err(struct gab_triple gab,
                                       struct gab_obj_block *b, uint8_t *ip,
                                       bool has_parent, enum gab_status s,
                                       const char *fmt) {

  gab_value message = has_parent ? compute_message_from_ip(b, ip) : gab_nil;

  if (b) {

    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

    size_t tok = compute_token_from_ip(b, ip);

    return (struct gab_err_argt){
        .tok = tok,
        .src = p->src,
        .note_fmt = fmt,
        .status = s,
        .message = message,
    };
  }

  return (struct gab_err_argt){
      .note_fmt = fmt,
      .status = s,
      .message = message,
  };
}

a_gab_value *vvm_error(struct gab_triple gab, enum gab_status s,
                       const char *fmt, va_list va) {
  gab_value *f = gab.vm->fp;
  uint8_t *ip = gab.vm->ip;

  struct gab_triple dont_exit = gab;
  dont_exit.flags &= ~fGAB_ERR_EXIT;

  while (frame_parent(f) > gab.vm->sb) {
    gab_vfpanic(dont_exit, stderr, nullptr,
                vm_frame_build_err(gab, frame_block(f), ip,
                                   frame_parent(f) > gab.vm->sb, GAB_NONE, ""));

    ip = frame_ip(f);
    f = frame_parent(f);
  }

  gab_vfpanic(gab, stderr, va,
              vm_frame_build_err(gab, frame_block(f), ip, false, s, fmt));

  gab_value results[] = {
      gab_string(gab, gab_status_names[s]),
      gab_box(gab,
              (struct gab_box_argt){
                  .size = sizeof(struct gab_vm *),
                  .data = &gab.vm,
                  .type = gab_string(gab, "gab.vm"),
              }),
  };

  gab_niref(gab, 1, 2, results);

  return a_gab_value_create(results, sizeof(results) / sizeof(gab_value));
}

a_gab_value *vm_error(struct gab_triple gab, enum gab_status s, const char *fmt,
                      ...) {
  va_list va;
  va_start(va, fmt);

  a_gab_value *res = vvm_error(gab, s, fmt, va);

  va_end(va);

  return res;
}

#define FMT_TYPEMISMATCH                                                       \
  "found:\n\n >> $\n\nof type:\n\n >> $\n\nbut expected type:\n\n >> $"

#define FMT_MISSINGIMPL                                                        \
  "$ does not specialize for:\n\n >> $\n\nof type:\n\n >> $"

a_gab_value *gab_panic(struct gab_triple gab, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);

  a_gab_value *res = vvm_error(gab, GAB_PANIC, fmt, va);

  va_end(va);

  return res;
}

a_gab_value *gab_ptypemismatch(struct gab_triple gab, gab_value found,
                               gab_value texpected) {

  return vm_error(gab, GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, found,
                  gab_valtype(gab.eg, found), texpected);
}

void gab_vmcreate(struct gab_vm *self, size_t argc, gab_value argv[argc]) {
  self->fp = self->sb + 3; // Is 4 too many?
  self->sp = self->sb + 3;
}

void gab_vmdestroy(struct gab_eg *gab, struct gab_vm *self) {}

gab_value gab_vmframe(struct gab_triple gab, uint64_t depth) {
  // uint64_t frame_count = gab.vm->fp - gab.vm->sb;
  //
  // if (depth >= frame_count)
  return gab_undefined;

  // struct gab_vm_frame *f = gab.vm->fp - depth;
  //
  // const char *keys[] = {
  //     "line",
  // };
  //
  // gab_value line = gab_nil;
  //
  // if (f->b) {
  //   struct gab_src *src = GAB_VAL_TO_PROTOTYPE(f->b->p)->src;
  //   size_t tok = compute_token_from_ip(f);
  //   line = gab_number(v_uint64_t_val_at(&src->token_lines, tok));
  // }
  //
  // gab_value values[] = {
  //     line,
  // };
  //
  // size_t len = sizeof(keys) / sizeof(keys[0]);
  //
  // return gab_srecord(gab, len, keys, values);
}

void gab_fvminspect(FILE *stream, struct gab_vm *vm, int depth) {
  // uint64_t frame_count = vm->fp - vm->fb;
  //
  // if (value > frame_count)
  // return;

  // struct gab_vm_frame *f = vm->fp - value;
  //
  // struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(f->b->p);
  //
  // fprintf(stream,
  //         GAB_GREEN " %03lu" GAB_RESET " closure:" GAB_CYAN "%-20s" GAB_RESET
  //                   " %d upvalues\n",
  //         frame_count - value, gab_strdata(&p->src->name), p->nupvalues);

  gab_value *f = vm->fp;
  gab_value *t = vm->sp - 1;

  while (depth > 0) {
    if (frame_parent(f) > vm->sb) {
      t = f - 3;
      f = frame_parent(f);
    }
    depth--;
  }

  gab_fvalinspect(stream, __gab_obj(frame_block(f)), 0);
  fprintf(stream, "\n");

  while (t >= f) {
    fprintf(stream, "%2s" GAB_YELLOW "%4lu " GAB_RESET, vm->sp == t ? "->" : "",
            t - f);
    gab_fvalinspect(stream, *t, 0);
    fprintf(stream, "\n");
    t--;
  }
}

static inline size_t compute_arity(size_t var, uint8_t have) {
  return var * (have & fHAVE_VAR) + (have >> 2);
}

static inline bool has_callspace(struct gab_vm *vm, size_t space_needed) {
  if (vm->sp - vm->sb + space_needed >= cGAB_STACK_MAX) {
    return false;
  }

  return true;
}

inline size_t gab_nvmpush(struct gab_vm *vm, uint64_t argc,
                          gab_value argv[argc]) {
  if (__gab_unlikely(argc == 0 || !has_callspace(vm, argc))) {
    return 0;
  }

  for (uint8_t n = 0; n < argc; n++) {
    *vm->sp++ = argv[n];
  }

  return argc;
}

#define OP_TRIM_N(n) ((uint16_t)OP_TRIM << 8 | (n))

/* Branchlessley skip the next instruction if the trim amount is matched */
// IP() += 2 * (n < 255 && PREVIEW_SHORT == OP_TRIM_N(n));

#define SET_VAR(n) ({ VAR() = n; })

#define CALL_BLOCK(blk, have)                                                  \
  ({                                                                           \
    PUSH_FRAME(blk, have);                                                     \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);                \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), p->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = p->src->bytecode.data + p->offset;                                  \
    KB() = p->src->constants.data;                                             \
    FB() = SP() - have;                                                        \
                                                                               \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

#define LOCALCALL_BLOCK(blk, have)                                             \
  ({                                                                           \
    PUSH_FRAME(blk, have);                                                     \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);                \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), p->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = ((void *)ks[GAB_SEND_KOFFSET]);                                     \
    FB() = SP() - have;                                                        \
                                                                               \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

#define TAILCALL_BLOCK(blk, have)                                              \
  ({                                                                           \
    gab_value *from = SP() - have;                                             \
    gab_value *to = FB();                                                      \
                                                                               \
    memmove(to, from, have * sizeof(gab_value));                               \
    SP() = to + have;                                                          \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);                \
                                                                               \
    IP() = p->src->bytecode.data + p->offset;                                  \
    KB() = p->src->constants.data;                                             \
                                                                               \
    SET_BLOCK(blk);                                                            \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

#define LOCALTAILCALL_BLOCK(blk, have)                                         \
  ({                                                                           \
    gab_value *from = SP() - have;                                             \
    gab_value *to = FB();                                                      \
                                                                               \
    memmove(to, from, have * sizeof(gab_value));                               \
    SP() = to + have;                                                          \
                                                                               \
    IP() = ((void *)ks[GAB_SEND_KOFFSET]);                                     \
                                                                               \
    SET_BLOCK(blk);                                                            \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

#define PROPERTY_RECORD(r, m, have)                                            \
  ({                                                                           \
    switch (have) {                                                            \
    case 1:                                                                    \
      PEEK() = gab_mapat(r, m);                                                \
      break;                                                                   \
    default:                                                                   \
      DROP_N((have) - 1);                                                      \
    case 2: {                                                                  \
      gab_value value = gab_mapput(GAB(), r, m, POP());                        \
      PEEK() = value;                                                          \
      break;                                                                   \
    }                                                                          \
    }                                                                          \
                                                                               \
    SET_VAR(1);                                                                \
                                                                               \
    NEXT();                                                                    \
  })

#define CALL_NATIVE(native, have, message)                                     \
  ({                                                                           \
    STORE();                                                                   \
                                                                               \
    gab_value *to = SP() - have;                                               \
                                                                               \
    gab_value *before = SP();                                                  \
                                                                               \
    size_t pass = message ? have : have - 1;                                   \
                                                                               \
    a_gab_value *res = (*native->function)(GAB(), pass, SP() - pass);          \
    if (__gab_unlikely(res))                                                   \
      return res;                                                              \
                                                                               \
    SP() = VM()->sp;                                                           \
                                                                               \
    assert(SP() >= before);                                                    \
    uint64_t have = SP() - before;                                             \
                                                                               \
    if (!have)                                                                 \
      PUSH(gab_nil), have++;                                                   \
                                                                               \
    memmove(to, before, have * sizeof(gab_value));                             \
    SP() = to + have;                                                          \
                                                                               \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

static inline gab_value block(struct gab_triple gab, gab_value p,
                              gab_value *locals, gab_value *upvs) {
  gab_value blk = gab_block(gab, p);

  struct gab_obj_block *b = GAB_VAL_TO_BLOCK(blk);
  struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(p);

  for (int i = 0; i < proto->nupvalues; i++) {
    uint8_t is_local = proto->data[i] & fLOCAL_LOCAL;
    uint8_t index = proto->data[i] >> 1;

    if (is_local)
      b->upvalues[i] = locals[index];
    else
      b->upvalues[i] = upvs[index];
  }

  return blk;
}

a_gab_value *error(OP_HANDLER_ARGS) {
  uint64_t have = *VM()->sp;
  gab_value *from = VM()->sp - have;

  a_gab_value *results = a_gab_value_empty(have);
  memcpy(results->data, from, have * sizeof(gab_value));

  gab_niref(GAB(), 1, results->len, results->data);

  return results;
}

a_gab_value *ok(OP_HANDLER_ARGS) {
  uint64_t have = *VM()->sp;
  gab_value *from = VM()->sp - have;

  a_gab_value *results = a_gab_value_empty(have + 1);
  results->data[0] = gab_ok;
  memcpy(results->data + 1, from, have * sizeof(gab_value));

  gab_niref(GAB(), 1, results->len, results->data);

  VM()->sp = VM()->sb;

  gab_vmdestroy(EG(), VM());
  free(VM());

  return results;
}

a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args) {
  gab.flags = args.flags;

  if (gab.flags & fGAB_BUILD_CHECK)
    return nullptr;

  gab.vm = malloc(sizeof(struct gab_vm));
  gab_vmcreate(gab.vm, args.len, args.argv);

  *gab.vm->sp++ = args.main;
  for (uint8_t i = 0; i < args.len; i++)
    *gab.vm->sp++ = args.argv[i];

  *gab.vm->sp = args.len;

  if (gab_valkind(args.main) != kGAB_BLOCK)
    return nullptr;

  struct gab_obj_block *b = GAB_VAL_TO_BLOCK(args.main);
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

  uint8_t *ip = p->src->bytecode.data + p->offset;
  uint8_t op = *ip++;

  gab.vm->fp[-1] = (uintptr_t)gab.vm->sb - 1;
  gab.vm->fp[-2] = 0;
  gab.vm->fp[-3] = (uintptr_t)b;

  return handlers[op](gab, ip, p->src->constants.data, gab.vm->fp, gab.vm->sp);
}

#define ERROR_GUARD_KIND(value, kind)                                          \
  if (__gab_unlikely(gab_valkind(value) != kind)) {                            \
    STORE_PRIMITIVE_ERROR_FRAME(1);                                            \
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, value,                          \
          gab_valtype(EG(), value), gab_egtype(EG(), kind));                   \
  }

#define ERROR_GUARD_ISB(value)                                                 \
  if (__gab_unlikely(!__gab_valisb(value))) {                                  \
    STORE_PRIMITIVE_ERROR_FRAME(have1);                                        \
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, value,                          \
          gab_valtype(EG(), value), gab_egtype(EG(), kGAB_SIGIL));             \
  }

#define ERROR_GUARD_ISN(value)                                                 \
  if (__gab_unlikely(!__gab_valisn(value))) {                                  \
    STORE_PRIMITIVE_ERROR_FRAME(have1);                                        \
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, value,                          \
          gab_valtype(EG(), value), gab_egtype(EG(), kGAB_NUMBER));            \
  }

#define ERROR_GUARD_ISS(value)                                                 \
  if (__gab_unlikely(gab_valkind(value) != kGAB_STRING &&                      \
                     gab_valkind(value) != kGAB_SIGIL)) {                      \
    STORE_PRIMITIVE_ERROR_FRAME(have);                                         \
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, value,                          \
          gab_valtype(EG(), value), gab_egtype(EG(), kGAB_STRING));            \
  }

#define SEND_GUARD(clause)                                                     \
  if (__gab_unlikely(!(clause)))                                               \
    MISS_CACHED_SEND();

#define SEND_GUARD_KIND(r, k) SEND_GUARD(gab_valkind(r) == k)

#define SEND_GUARD_CACHED_MESSAGE_SPECS(m)                                     \
  SEND_GUARD(gab_valeq(gab_egmsgrec(EG(), m), ks[GAB_SEND_KSPECS]))

#define SEND_GUARD_CACHED_RECEIVER_TYPE(r)                                     \
  SEND_GUARD(gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE]))

#define SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m)                                \
  SEND_GUARD(gab_valeq(gab_egmsgrec(EG(), m), ks[GAB_SEND_KGENERIC_CALL_SPECS]))

CASE_CODE(MATCHTAILSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];
  gab_value t = gab_valtype(EG(), r);

  SEND_GUARD_CACHED_MESSAGE_SPECS(m)

  uint8_t idx = GAB_SEND_HASH(t) * GAB_SEND_CACHE_SIZE;

  // TODO: Handle undefined and record case
  if (__gab_unlikely(ks[GAB_SEND_KTYPE + idx] != t))
    MISS_CACHED_SEND();

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC + idx];

  gab_value *from = SP() - have;
  gab_value *to = FB();

  memmove(to, from, have * sizeof(gab_value));

  IP() = (void *)ks[GAB_SEND_KOFFSET + idx];
  SP() = to + have;

  SET_BLOCK(b);
  SET_VAR(have);

  NEXT();
}

CASE_CODE(MATCHSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];
  gab_value t = gab_valtype(EG(), r);

  SEND_GUARD_CACHED_MESSAGE_SPECS(m)

  uint8_t idx = GAB_SEND_HASH(t) * GAB_SEND_CACHE_SIZE;

  // TODO: Handle undefined and record case
  if (__gab_unlikely(ks[GAB_SEND_KTYPE + idx] != t))
    MISS_CACHED_SEND();

  struct gab_obj_block *blk = (void *)ks[GAB_SEND_KSPEC + idx];

  PUSH_FRAME(blk, have);

  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);

  if (__gab_unlikely(!has_callspace(VM(), p->nslots - have)))
    ERROR(GAB_OVERFLOW, "");

  IP() = (void *)ks[GAB_SEND_KOFFSET + idx];
  FB() = SP() - have;

  SET_VAR(have);

  NEXT();
}

static inline bool try_setup_localmatch(struct gab_eg *eg, gab_value m,
                                        gab_value *ks,
                                        struct gab_obj_prototype *p) {
  gab_value specs = gab_egmsgrec(eg, m);

  if (specs == gab_undefined)
    return false;

  if (gab_maplen(specs) > 4 || gab_maplen(specs) < 2)
    return false;

  for (size_t i = 0; i < gab_maplen(specs); i++) {
    gab_value spec = gab_uvmapat(specs, i);

    if (gab_valkind(spec) != kGAB_BLOCK)
      return false;

    struct gab_obj_block *b = GAB_VAL_TO_BLOCK(spec);
    struct gab_obj_prototype *spec_p = GAB_VAL_TO_PROTOTYPE(b->p);

    if (spec_p->src != p->src)
      return false;

    gab_value t = gab_ukmapat(specs, i);

    uint8_t idx = GAB_SEND_HASH(t) * GAB_SEND_CACHE_SIZE;

    // We have a collision - no point in messing about with this.
    if (ks[GAB_SEND_KSPEC + idx] != gab_undefined)
      return false;

    uint8_t *ip = p->src->bytecode.data + spec_p->offset;

    ks[GAB_SEND_KTYPE + idx] = t;
    ks[GAB_SEND_KSPEC + idx] = (intptr_t)b;
    ks[GAB_SEND_KOFFSET + idx] = (intptr_t)ip;
  }

  ks[GAB_SEND_KSPECS] = specs;
  return true;
}

CASE_CODE(LOAD_UPVALUE) {
  PUSH(UPVALUE(READ_BYTE));

  NEXT();
}

CASE_CODE(NLOAD_UPVALUE) {
  uint8_t n = READ_BYTE;

  while (n--)
    PUSH(UPVALUE(READ_BYTE));

  NEXT();
}

CASE_CODE(LOAD_LOCAL) {
  PUSH(LOCAL(READ_BYTE));

  NEXT();
}

CASE_CODE(NLOAD_LOCAL) {
  uint8_t n = READ_BYTE;

  while (n--)
    PUSH(LOCAL(READ_BYTE));

  NEXT();
}

CASE_CODE(STORE_LOCAL) {
  LOCAL(READ_BYTE) = PEEK();

  NEXT();
}

CASE_CODE(NPOPSTORE_STORE_LOCAL) {
  uint8_t n = READ_BYTE - 1;

  while (n--)
    LOCAL(READ_BYTE) = POP();

  LOCAL(READ_BYTE) = PEEK();

  NEXT();
}

CASE_CODE(POPSTORE_LOCAL) {
  LOCAL(READ_BYTE) = POP();

  NEXT();
}

CASE_CODE(NPOPSTORE_LOCAL) {
  uint8_t n = READ_BYTE;

  while (n--)
    LOCAL(READ_BYTE) = POP();

  NEXT();
}

CASE_CODE(SEND_NATIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  struct gab_obj_native *n = (void *)ks[GAB_SEND_KSPEC];

  CALL_NATIVE(n, have, true);
}

CASE_CODE(SEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC];

  CALL_BLOCK(b, have);
}

CASE_CODE(TAILSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC];

  TAILCALL_BLOCK(b, have);
}

CASE_CODE(LOCALSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC];

  LOCALCALL_BLOCK(b, have);
}

CASE_CODE(LOCALTAILSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC];

  LOCALTAILCALL_BLOCK(b, have);
}

CASE_CODE(SEND_PRIMITIVE_CALL_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  ERROR_GUARD_KIND(r, kGAB_BLOCK);

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(r);

  CALL_BLOCK(blk, have);
}

CASE_CODE(TAILSEND_PRIMITIVE_CALL_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  ERROR_GUARD_KIND(r, kGAB_BLOCK);

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(r);

  TAILCALL_BLOCK(blk, have);
}

CASE_CODE(SEND_PRIMITIVE_CALL_NATIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  ERROR_GUARD_KIND(r, kGAB_NATIVE);

  struct gab_obj_native *n = GAB_VAL_TO_NATIVE(r);

  CALL_NATIVE(n, have, false);
}

IMPL_SEND_UNARY_NUMERIC(PRIMITIVE_BIN, gab_number, uint64_t, ~);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_ADD, gab_number, double, +);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_SUB, gab_number, double, -);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_MUL, gab_number, double, *);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_DIV, gab_number, double, /);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_MOD, gab_number, uint64_t, %);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_BOR, gab_number, uint64_t, |);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_BND, gab_number, uint64_t, &);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_LSH, gab_number, uint64_t, <<);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_RSH, gab_number, uint64_t, >>);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_LT, gab_bool, double, <);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_LTE, gab_bool, double, >=);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_GT, gab_bool, double, >);
IMPL_SEND_BINARY_NUMERIC(PRIMITIVE_GTE, gab_bool, double, >=);
IMPL_SEND_UNARY_BOOLEAN(PRIMITIVE_LIN, gab_bool, bool, !);
IMPL_SEND_BINARY_BOOLEAN(PRIMITIVE_LOR, gab_bool, bool, ||);
IMPL_SEND_BINARY_BOOLEAN(PRIMITIVE_LND, gab_bool, bool, &&);

CASE_CODE(SEND_PRIMITIVE_EQ) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  if (__gab_unlikely(have < 2))
    PUSH(gab_nil), have++;

  gab_value a = PEEK_N(have);
  gab_value b = PEEK_N(have - 1);

  DROP_N(have);
  PUSH(gab_bool(gab_valeq(a, b)));

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_CONCAT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  SEND_GUARD_CACHED_RECEIVER_TYPE(PEEK_N(have));

  if (__gab_unlikely(have < 2))
    PUSH(gab_nil), have++;

  ERROR_GUARD_ISS(PEEK_N(have));
  ERROR_GUARD_ISS(PEEK_N(have - 1));

  STORE_SP();

  gab_value val_a = PEEK_N(have);
  gab_value val_b = PEEK_N(have - 1);
  gab_value val_ab = gab_strcat(GAB(), val_a, val_b);

  DROP_N(have);
  PUSH(val_ab);

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_USE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  SEND_GUARD_KIND(r, kGAB_STRING);

  STORE();
  a_gab_value *mod = gab_use(GAB(), r);

  DROP_N(have);

  if (!mod)
    ERROR(GAB_PANIC, "Couldn't locate module $.", r);

  for (size_t i = 1; i < mod->len; i++)
    PUSH(mod->data[i]);

  SET_VAR(mod->len - 1);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SPLAT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  SEND_GUARD_KIND(r, kGAB_MAP);

  DROP_N(have);

  size_t len = gab_maplen(r);

  for (size_t i = 0; i < len; i++) {
    PUSH(gab_uvmapat(r, i));
  }

  SET_VAR(len);

  NEXT();
}

CASE_CODE(SEND_CONSTANT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_MESSAGE_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  gab_value spec = ks[GAB_SEND_KSPEC];

  DROP_N(have);

  PUSH(spec);

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PROPERTY_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_RECEIVER_TYPE(r)

  gab_value spec = gab_mapat(r, m);

  SEND_GUARD_KIND(spec, kGAB_BLOCK);

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

  CALL_BLOCK(blk, have);
}

CASE_CODE(TAILSEND_PROPERTY_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_RECEIVER_TYPE(r)

  gab_value spec = gab_mapat(r, m);

  SEND_GUARD_KIND(spec, kGAB_BLOCK);

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

  TAILCALL_BLOCK(blk, have);
}

CASE_CODE(SEND_PROPERTY_NATIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_RECEIVER_TYPE(r)

  gab_value spec = gab_mapat(r, m);

  SEND_GUARD_KIND(spec, kGAB_NATIVE);

  struct gab_obj_native *native = GAB_VAL_TO_NATIVE(spec);

  CALL_NATIVE(native, have, true);
}

CASE_CODE(SEND_PROPERTY_PRIMITIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_RECEIVER_TYPE(r)

  gab_value spec = gab_mapat(r, m);

  SEND_GUARD_KIND(spec, kGAB_PRIMITIVE);

  uint8_t op = gab_valtop(spec);

  IP() -= SEND_CACHE_DIST - 1;

  DISPATCH(op);
}

CASE_CODE(SEND_PROPERTY_CONSTANT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_RECEIVER_TYPE(r)

  PROPERTY_RECORD(r, m, have);
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  gab_value spec = gab_mapat(r, m);
  SEND_GUARD_KIND(spec, kGAB_BLOCK);

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  CALL_BLOCK(blk, have);
}

CASE_CODE(TAILSEND_PRIMITIVE_SEND_GENERIC_PROPERTY_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(
      r); // This isn't safe anymore. Receiver type is always 'map'

  gab_value spec = gab_mapat(r, m);
  SEND_GUARD_KIND(spec, kGAB_BLOCK);

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  TAILCALL_BLOCK(blk, have);
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_CONSTANT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  PROPERTY_RECORD(r, m, have);
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_PRIMITIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  gab_value spec = gab_mapat(r, m);
  SEND_GUARD_KIND(spec, kGAB_PRIMITIVE);

  uint8_t op = gab_valtop(spec);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  PEEK() = gab_nil;

  IP() -= SEND_CACHE_DIST - 1;
  DISPATCH(op);
}
CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_NATIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint8_t have_byte = READ_BYTE;
  uint64_t have = compute_arity(VAR(), have_byte);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  gab_value spec = gab_mapat(r, m);
  SEND_GUARD_KIND(spec, kGAB_NATIVE);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  struct gab_obj_native *native = GAB_VAL_TO_NATIVE(spec);

  CALL_NATIVE(native, have, true);
}

CASE_CODE(SEND_PROPERTY) {
  gab_value *ks = READ_CONSTANTS;
  uint8_t have_byte = READ_BYTE;
  uint64_t have = compute_arity(VAR(), have_byte);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  SEND_GUARD_CACHED_RECEIVER_TYPE(r)

  gab_value spec = gab_mapat(r, m);

  switch (gab_valkind(spec)) {
  case kGAB_PRIMITIVE:
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PROPERTY_PRIMITIVE);
    break;
  case kGAB_NATIVE:
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PROPERTY_NATIVE);
    break;
  case kGAB_BLOCK: {
    uint8_t adjust = (have_byte & fHAVE_TAIL) >> 1;

    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PROPERTY_BLOCK + adjust);
    break;
  }
  default:
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PROPERTY_CONSTANT);
    break;
  }

  IP() -= SEND_CACHE_DIST;
  NEXT();
}

CASE_CODE(RETURN) {
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value *from = SP() - have;
  gab_value *to = FB() - 3;

  if (__gab_unlikely(RETURN_FB() < VM()->sb))
    return STORE(), SET_VAR(have), ok(DISPATCH_ARGS());

  LOAD_FRAME();

  memmove(to, from, have * sizeof(gab_value));
  SP() = to + have;

  SET_VAR(have);

  NEXT();
}

CASE_CODE(NOP) { NEXT(); }

CASE_CODE(CONSTANT) {
  PUSH(READ_CONSTANT);

  NEXT();
}

CASE_CODE(NCONSTANT) {
  uint8_t n = READ_BYTE;

  while (n--)
    PUSH(READ_CONSTANT);

  NEXT();
}

CASE_CODE(SHIFT) {
  uint8_t n = READ_BYTE;

  gab_value tmp = PEEK();

  memmove(SP() - (n - 1), SP() - n, (n - 1) * sizeof(gab_value));

  PEEK_N(n) = tmp;

  NEXT();
}

CASE_CODE(POP) {
  DROP();

  NEXT();
}

CASE_CODE(POP_N) {
  DROP_N(READ_BYTE);

  NEXT();
}

CASE_CODE(INTERPOLATE) {
  uint8_t n = READ_BYTE;

  STORE_SP();
  gab_value str = gab_valintos(GAB(), PEEK_N(n));

  // tODO: UHOH CAN THIS TAILCALL? STR takes addresses
  for (uint8_t i = n - 1; i > 0; i--) {
    gab_value curr = gab_valintos(GAB(), PEEK_N(i));
    str = gab_strcat(GAB(), str, curr);
  }

  POP_N(n);

  PUSH(str);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_TYPE) {
  SKIP_SHORT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  PEEK_N(have) = gab_valtype(EG(), PEEK_N(have));

  DROP_N(have - 1);

  SET_VAR(1);

  NEXT();
}

CASE_CODE(BLOCK) {
  gab_value p = READ_CONSTANT;

  STORE_SP();

  gab_value blk = block(GAB(), p, FB(), BLOCK()->upvalues);

  PUSH(blk);

  NEXT();
}

TRIM_N(0)
TRIM_N(1)
TRIM_N(2)
TRIM_N(3)
TRIM_N(4)
TRIM_N(5)
TRIM_N(6)
TRIM_N(7)
TRIM_N(8)
TRIM_N(9)

CASE_CODE(TRIM) {
  uint8_t want = READ_BYTE;
  uint64_t have = VAR();
  uint64_t nulls = 0;

  if (have == want && want < 10) {
    WRITE_BYTE(2, OP_TRIM_EXACTLY0 + want);

    IP() -= 2;

    NEXT();
  }

  if (have > want && have - want < 10) {
    WRITE_BYTE(2, OP_TRIM_DOWN1 - 1 + (have - want));

    IP() -= 2;

    NEXT();
  }

  if (want > have && want - have < 10) {
    WRITE_BYTE(2, OP_TRIM_UP1 - 1 + (want - have));

    IP() -= 2;

    NEXT();
  }

  SP() -= have;

  if (__gab_unlikely(have != want && want != VAR_EXP)) {
    if (have > want) {
      have = want;
    } else {
      nulls = want - have;
    }
  }

  SP() += have + nulls;
  VAR() = have + nulls;

  while (nulls--)
    PEEK_N(nulls + 1) = gab_nil;

  NEXT();
}

CASE_CODE(PACK) {
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t below = READ_BYTE;
  uint8_t above = READ_BYTE;

  uint64_t want = below + above;

  while (have < want)
    PUSH(gab_nil), have++;

  uint64_t len = have - want;

  gab_value *ap = SP() - above;

  STORE_SP();

  gab_value rec = gab_tuple(GAB(), len, ap - len);

  DROP_N(len - 1);

  memmove(ap - len + 1, ap, above * sizeof(gab_value));

  PEEK_N(above + 1) = rec;

  SET_VAR(want + 1);

  NEXT();
}

CASE_CODE(RECORD) {
  uint8_t len = READ_BYTE;

  gab_gclock(GC());

  gab_value map = gab_map(GAB(), 2, len, SP() - len * 2, SP() + 1 - (len * 2));

  DROP_N(len * 2);

  PUSH(map);

  STORE_SP();

  gab_gcunlock(GC());

  NEXT();
}

CASE_CODE(TUPLE) {
  uint64_t len = compute_arity(VAR(), READ_BYTE);

  gab_gclock(GC());

  gab_value rec = gab_tuple(GAB(), len, SP() - len);

  DROP_N(len);

  PUSH(rec);

  STORE_SP();

  gab_gcunlock(GC());

  NEXT();
}

CASE_CODE(SEND) {
  gab_value *ks = READ_CONSTANTS;
  uint8_t have_byte = READ_BYTE;
  uint64_t have = compute_arity(VAR(), have_byte);

  uint8_t adjust = (have_byte & fHAVE_TAIL) >> 1;

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  if (try_setup_localmatch(EG(), m, ks, BLOCK_PROTO())) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_MATCHSEND_BLOCK + adjust);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  /* Do the expensive lookup */
  struct gab_egimpl_rest res = gab_egimpl(EG(), m, r);

  if (__gab_unlikely(!res.status)) {
    STORE();
    ERROR(GAB_IMPLEMENTATION_MISSING, FMT_MISSINGIMPL, m, r,
          gab_valtype(EG(), r));
  }

  gab_value spec = res.status == kGAB_IMPL_PROPERTY
                       ? gab_primitive(OP_SEND_PROPERTY)
                       : res.spec;

  ks[GAB_SEND_KSPECS] = gab_egmsgrec(EG(), m);
  ks[GAB_SEND_KTYPE] = gab_valtype(EG(), r);
  ks[GAB_SEND_KSPEC] =
      res.status == kGAB_IMPL_PROPERTY ? GAB_VAL_TO_MAP(r)->hash : res.spec;

  switch (gab_valkind(spec)) {
  case kGAB_PRIMITIVE: {
    uint8_t op = gab_valtop(spec);

    if (op == OP_SEND_PRIMITIVE_CALL_BLOCK)
      op += adjust;

    WRITE_BYTE(SEND_CACHE_DIST, op);

    break;
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *b = GAB_VAL_TO_BLOCK(spec);
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

    uint8_t local = (GAB_VAL_TO_PROTOTYPE(b->p)->src == BLOCK_PROTO()->src);
    adjust |= (local << 1);

    if (local) {
      ks[GAB_SEND_KOFFSET] = (intptr_t)p->src->bytecode.data + p->offset;
    }

    ks[GAB_SEND_KSPEC] = (intptr_t)b;
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_BLOCK + adjust);

    break;
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(spec);

    ks[GAB_SEND_KSPEC] = (intptr_t)n;
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_NATIVE);

    break;
  }
  default:
    ks[GAB_SEND_KSPEC] = spec;
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_CONSTANT);
    break;
  }

  IP() -= SEND_CACHE_DIST;

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  struct gab_obj_block *spec = (void *)ks[GAB_SEND_KSPEC];

  CALL_BLOCK(spec, have);
}

CASE_CODE(TAILSEND_PRIMITIVE_SEND_GENERIC_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  struct gab_obj_block *spec = (void *)ks[GAB_SEND_KSPEC];

  TAILCALL_BLOCK(spec, have);
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_NATIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  have--;
  DROP();

  struct gab_obj_native *spec = (void *)ks[GAB_SEND_KSPEC];

  CALL_NATIVE(spec, have, true);
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_PRIMITIVE) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  memmove(SP() - (have - 1), SP() - (have - 2), (have - 1) * sizeof(gab_value));
  PEEK() = gab_nil;

  uint8_t spec = ks[GAB_SEND_KSPEC];

  uint8_t op = gab_valtop(spec);

  IP() -= SEND_CACHE_DIST - 1;

  DISPATCH(op);
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC_CONSTANT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);

  SEND_GUARD_KIND(m, kGAB_MESSAGE);
  SEND_GUARD_CACHED_GENERIC_CALL_SPECS(m);
  SEND_GUARD_CACHED_RECEIVER_TYPE(r);

  gab_value spec = ks[GAB_SEND_KSPEC];

  DROP_N(have);

  PUSH(spec);

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SEND_GENERIC) {
  gab_value *ks = READ_CONSTANTS;
  uint8_t have_byte = READ_BYTE;
  uint64_t have = compute_arity(VAR(), have_byte);

  gab_value r = PEEK_N(have);
  gab_value m = PEEK_N(have - 1);
  gab_value t = gab_valtype(EG(), r);

  ERROR_GUARD_KIND(m, kGAB_MESSAGE);

  struct gab_egimpl_rest res = gab_egimpl(EG(), m, r);

  if (__gab_unlikely(!res.status)) {
    STORE();
    ERROR(GAB_IMPLEMENTATION_MISSING, FMT_MISSINGIMPL, m, r,
          gab_valtype(EG(), r));
  }

  ks[GAB_SEND_KTYPE] = t;
  ks[GAB_SEND_KSPEC] = res.spec;
  ks[GAB_SEND_KGENERIC_CALL_SPECS] = gab_egmsgrec(EG(), m);
  ks[GAB_SEND_KGENERIC_CALL_MESSAGE] = m;

  if (res.status == kGAB_IMPL_PROPERTY) {
    gab_value spec = res.spec;

    switch (gab_valkind(spec)) {
    case kGAB_PRIMITIVE:
      WRITE_BYTE(SEND_CACHE_DIST,
                 OP_SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_PRIMITIVE);
      break;
    case kGAB_NATIVE:
      WRITE_BYTE(SEND_CACHE_DIST,
                 OP_SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_NATIVE);
      break;
    case kGAB_BLOCK: {
      uint8_t adjust = (have_byte & fHAVE_TAIL) >> 1;

      WRITE_BYTE(SEND_CACHE_DIST,
                 OP_SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_BLOCK + adjust);
      break;
    }
    default:
      WRITE_BYTE(SEND_CACHE_DIST,
                 OP_SEND_PRIMITIVE_SEND_GENERIC_PROPERTY_CONSTANT);
      break;
    }
  } else {
    gab_value spec = res.spec;

    switch (gab_valkind(spec)) {
    case kGAB_PRIMITIVE: {
      ks[GAB_SEND_KSPEC] = gab_valtop(spec);

      WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PRIMITIVE_SEND_GENERIC_PRIMITIVE);
      break;
    }
    case kGAB_BLOCK: {
      ks[GAB_SEND_KSPEC] = (uintptr_t)GAB_VAL_TO_BLOCK(spec);

      uint8_t adjust = (have_byte & fHAVE_TAIL) >> 1;

      WRITE_BYTE(SEND_CACHE_DIST,
                 OP_SEND_PRIMITIVE_SEND_GENERIC_BLOCK + adjust);
      break;
    }
    case kGAB_NATIVE: {
      ks[GAB_SEND_KSPEC] = (uintptr_t)GAB_VAL_TO_NATIVE(spec);

      WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PRIMITIVE_SEND_GENERIC_NATIVE);
      break;
    }
    default: {
      ks[GAB_SEND_KSPEC] = spec;

      WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PRIMITIVE_SEND_GENERIC_CONSTANT);
      break;
    }
    }
  }

  IP() -= SEND_CACHE_DIST;
  NEXT();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef IMPL_SEND_BINARY_PRIMITIVE
