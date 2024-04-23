#include "core.h"
#include "gab.h"

#define GAB_STATUS_NAMES_IMPL
#include "engine.h"

#include "colors.h"
#include "lexer.h"

#include <stdarg.h>
#include <string.h>

#define OP_HANDLER_ARGS                                                        \
  struct gab_triple __gab, uint8_t *__ip, gab_value *__kb,                     \
      struct gab_obj_block *__b, gab_value *__fb, gab_value *__sp

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

#define DISPATCH_ARGS() GAB(), IP(), KB(), BLOCK(), FB(), SP()

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
#define FRAME() (VM()->fp)
#define BLOCK() (__b)
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
    SKIP_SHORT;                                                                \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    if (__gab_unlikely(!__gab_valisn(PEEK_N(have))))                           \
      MISS_CACHED_SEND();                                                      \
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
    SKIP_SHORT;                                                                \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    if (__gab_unlikely(!__gab_valisn(PEEK_N(have))))                           \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    if (__gab_unlikely(have < 2))                                              \
      PUSH(gab_nil), have++;                                                   \
                                                                               \
    if (__gab_unlikely(!__gab_valisn(PEEK_N(have - 1)))) {                     \
      STORE_PRIMITIVE_ERROR_FRAME(have);                                       \
      ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, PEEK_N(have - 1),             \
            gab_valtype(EG(), PEEK_N(have - 1)),                               \
            gab_valtype(EG(), PEEK_N(have)));                                  \
    }                                                                          \
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

#define IMPL_SEND_UNARY_BOOLEAN(CODE, value_type, operation_type, operation)   \
  CASE_CODE(SEND_##CODE) {                                                     \
    SKIP_SHORT;                                                                \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    if (__gab_unlikely(!__gab_valisb(PEEK_N(have))))                           \
      MISS_CACHED_SEND();                                                      \
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
    SKIP_SHORT;                                                                \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    if (__gab_unlikely(!__gab_valisb(PEEK_N(have))))                           \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    if (__gab_unlikely(have < 2))                                              \
      PUSH(gab_nil), have++;                                                   \
                                                                               \
    if (__gab_unlikely(!__gab_valisb(PEEK_N(have - 1)))) {                     \
      STORE_PRIMITIVE_ERROR_FRAME(1);                                          \
      ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, PEEK_N(have - 1),             \
            gab_valtype(EG(), PEEK_N(have - 1)),                               \
            gab_valtype(EG(), PEEK_N(have)));                                  \
    }                                                                          \
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
#define STORE_FRAME()                                                          \
  ({                                                                           \
    FRAME()->b = BLOCK();                                                      \
    FRAME()->ip = IP();                                                        \
    FRAME()->slots = FB();                                                     \
  })

#define PUSH_FRAME()                                                           \
  ({                                                                           \
    FRAME()++;                                                                 \
    STORE_FRAME();                                                             \
  })

#define PUSH_ERROR_FRAME(have)                                                 \
  ({                                                                           \
    FRAME()++;                                                                 \
    FRAME()->b = nullptr;                                                      \
    FRAME()->ip = IP();                                                        \
    FRAME()->slots = SP() - have;                                              \
  })

#define POP_FRAME() ({ FRAME()--; })

#define STORE_PRIMITIVE_ERROR_FRAME(have)                                      \
  ({                                                                           \
    if (IP()[-1] & fHAVE_TAIL) {                                               \
      ;                                                                        \
    } else {                                                                   \
      PUSH_FRAME();                                                            \
    }                                                                          \
    PUSH_ERROR_FRAME(have);                                                    \
  })

#define LOAD_FRAME()                                                           \
  ({                                                                           \
    FB() = FRAME()->slots;                                                     \
    IP() = FRAME()->ip;                                                        \
    BLOCK() = FRAME()->b;                                                      \
    KB() = BLOCK_PROTO()->src->constants.data;                                 \
  })

#define SEND_CACHE_DIST 4

static inline size_t compute_token_from_ip(struct gab_vm_frame *f) {
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(f->b->p);

  size_t offset = f->ip - p->src->bytecode.data - 1;

  size_t token = v_uint64_t_val_at(&p->src->bytecode_toks, offset);

  return token;
}

static inline gab_value compute_message_from_ip(struct gab_vm_frame *f) {
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(f->b->p);

  uint16_t k = ((uint16_t)f->ip[-3] << 8) | f->ip[-2];

  gab_value m = v_gab_value_val_at(&p->src->constants, k);

  return m;
}

struct gab_err_argt vm_frame_build_err(struct gab_triple gab,
                                       struct gab_vm_frame *fp, bool has_parent,
                                       enum gab_status s, const char *fmt) {

  gab_value message = has_parent ? compute_message_from_ip(fp - 1) : gab_nil;

  if (fp->b) {

    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(fp->b->p);

    size_t tok = compute_token_from_ip(fp);

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
  struct gab_vm_frame *f = gab.vm->fb;

  struct gab_triple dont_exit = gab;
  dont_exit.flags &= ~fGAB_EXIT_ON_PANIC;

  while (f < gab.vm->fp) {
    gab_vfpanic(dont_exit, stderr, nullptr,
                vm_frame_build_err(gab, f, f > gab.vm->fb, GAB_NONE, ""));
    f++;
  }

  gab_vfpanic(gab, stderr, va,
              vm_frame_build_err(gab, f, f > gab.vm->fb, s, fmt));

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
  self->fp = self->fb - 1;
  self->sp = self->sb;
  self->fp->slots = self->sp;
}

void gab_vmdestroy(struct gab_eg *gab, struct gab_vm *self) {}

gab_value gab_vmframe(struct gab_triple gab, uint64_t depth) {
  uint64_t frame_count = gab.vm->fp - gab.vm->fb;

  if (depth >= frame_count)
    return gab_undefined;

  struct gab_vm_frame *f = gab.vm->fp - depth;

  const char *keys[] = {
      "line",
  };

  gab_value line = gab_nil;

  if (f->b) {
    struct gab_src *src = GAB_VAL_TO_PROTOTYPE(f->b->p)->src;
    size_t tok = compute_token_from_ip(f);
    line = gab_number(v_uint64_t_val_at(&src->token_lines, tok));
  }

  gab_value values[] = {
      line,
  };

  size_t len = sizeof(keys) / sizeof(keys[0]);

  return gab_srecord(gab, len, keys, values);
}

void gab_fvminspect(FILE *stream, struct gab_vm *vm, uint64_t value) {
  uint64_t frame_count = vm->fp - vm->fb;

  if (value > frame_count)
    return;

  struct gab_vm_frame *f = vm->fp - value;

  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(f->b->p);

  fprintf(stream,
          GAB_GREEN " %03lu" GAB_RESET " closure:" GAB_CYAN "%-20s" GAB_RESET
                    " %d upvalues\n",
          frame_count - value, gab_strdata(&p->src->name), p->nupvalues);

  size_t top = vm->sp - f->slots;

  for (int32_t i = top; i >= 0; i--) {
    fprintf(stream, "%2s" GAB_YELLOW "%4i " GAB_RESET,
            vm->sp == f->slots + i ? "->" : "", i);
    gab_fvalinspect(stream, f->slots[i], 0);
    fprintf(stream, "\n");
  }
}

static inline size_t compute_arity(size_t var, uint8_t have) {
  if (have & fHAVE_VAR)
    return var + (have >> 2);
  else
    return have >> 2;
}

static inline bool has_callspace(struct gab_vm *vm, size_t space_needed) {
  if (vm->fp - vm->fb + 1 >= cGAB_FRAMES_MAX) {
    return false;
  }

  if (vm->sp - vm->sb + space_needed >= cGAB_STACK_MAX) {
    return false;
  }

  return true;
}

inline size_t gab_vmpush(struct gab_vm *vm, gab_value value) {
  return gab_nvmpush(vm, 1, &value);
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
    PUSH_FRAME();                                                              \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);                \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), p->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = p->src->bytecode.data + p->offset;                                  \
    KB() = p->src->constants.data;                                             \
    BLOCK() = blk;                                                             \
    FB() = SP() - have;                                                        \
                                                                               \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

#define LOCALCALL_BLOCK(blk, have)                                             \
  ({                                                                           \
    PUSH_FRAME();                                                              \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(blk->p);                \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), p->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = ((void *)ks[GAB_SEND_KOFFSET]);                                     \
    BLOCK() = blk;                                                             \
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
    BLOCK() = blk;                                                             \
                                                                               \
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
    BLOCK() = blk;                                                             \
                                                                               \
    SET_VAR(have);                                                             \
                                                                               \
    NEXT();                                                                    \
  })

#define CALL_SUSPENSE(s, have)                                                 \
  ({                                                                           \
    PUSH_FRAME();                                                              \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(s->p);                  \
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(s->b);                        \
    struct gab_obj_prototype *bp = GAB_VAL_TO_PROTOTYPE(blk->p);               \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), s->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = p->src->bytecode.data + bp->offset + p->offset;                     \
    KB() = p->src->constants.data;                                             \
    BLOCK() = blk;                                                             \
    FB() = SP() - have;                                                        \
                                                                               \
    size_t arity = have - 1;                                                   \
                                                                               \
    gab_value *from = SP() - arity;                                            \
    gab_value *to = FB() + s->nslots;                                          \
                                                                               \
    memmove(to, from, arity * sizeof(gab_value));                              \
    SP() = to + arity;                                                         \
                                                                               \
    memcpy(FB(), s->slots, s->nslots * sizeof(gab_value));                     \
                                                                               \
    SET_VAR(arity);                                                            \
                                                                               \
    NEXT();                                                                    \
  })

#define LOCALCALL_SUSPENSE(s, have)                                            \
  ({                                                                           \
    PUSH_FRAME();                                                              \
                                                                               \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(s->p);                  \
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(s->b);                        \
    struct gab_obj_prototype *bp = GAB_VAL_TO_PROTOTYPE(blk->p);               \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), s->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = p->src->bytecode.data + bp->offset + p->offset;                     \
    KB() = p->src->constants.data;                                             \
    BLOCK() = blk;                                                             \
    FB() = SP() - have;                                                        \
                                                                               \
    size_t arity = have - 1;                                                   \
                                                                               \
    gab_value *from = SP() - arity;                                            \
    gab_value *to = FB() + s->nslots;                                          \
                                                                               \
    memmove(to, from, arity * sizeof(gab_value));                              \
    SP() = to + arity;                                                         \
                                                                               \
    memcpy(FB(), s->slots, s->nslots * sizeof(gab_value));                     \
                                                                               \
    SET_VAR(arity);                                                            \
                                                                               \
    NEXT();                                                                    \
  })

#define TAILCALL_SUSPENSE(s, have)                                             \
  ({                                                                           \
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(s->p);                  \
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(s->b);                        \
    struct gab_obj_prototype *bp = GAB_VAL_TO_PROTOTYPE(blk->p);               \
                                                                               \
    if (__gab_unlikely(!has_callspace(VM(), s->nslots - have)))                \
      ERROR(GAB_OVERFLOW, "");                                                 \
                                                                               \
    IP() = p->src->bytecode.data + bp->offset + p->offset;                     \
    KB() = p->src->constants.data;                                             \
    BLOCK() = blk;                                                             \
                                                                               \
    size_t arity = have - 1;                                                   \
                                                                               \
    gab_value *from = SP() - arity;                                            \
    gab_value *to = FB() + s->nslots;                                          \
                                                                               \
    memmove(to, from, arity * sizeof(gab_value));                              \
    SP() = to + arity;                                                         \
                                                                               \
    memcpy(FB(), s->slots, s->nslots * sizeof(gab_value));                     \
                                                                               \
    SET_VAR(arity);                                                            \
                                                                               \
    NEXT();                                                                    \
  })

#define CALL_NATIVE(native, have, message)                                     \
  ({                                                                           \
    STORE_SP();                                                                \
    PUSH_FRAME();                                                              \
    PUSH_ERROR_FRAME(have);                                                    \
                                                                               \
    gab_value *to = SP() - have;                                               \
                                                                               \
    gab_value *before = SP();                                                  \
                                                                               \
    size_t pass = message ? have : have - 1;                                   \
                                                                               \
    a_gab_value *res = (*n->function)(GAB(), pass, SP() - pass);               \
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
    POP_FRAME();                                                               \
    POP_FRAME();                                                               \
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
  results->data[0] = gab_string(GAB(), "ok");
  memcpy(results->data + 1, from, have * sizeof(gab_value));

  gab_niref(GAB(), 1, results->len, results->data);

  VM()->sp = VM()->sb;

  gab_vmdestroy(EG(), VM());
  free(VM());

  return results;
}

a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args) {
  gab.flags = args.flags;

  gab.vm = malloc(sizeof(struct gab_vm));
  gab_vmcreate(gab.vm, args.len, args.argv);

  *gab.vm->sp++ = args.main;
  for (uint8_t i = 0; i < args.len; i++)
    *gab.vm->sp++ = args.argv[i];

  *gab.vm->sp = args.len + 1;

  if (gab_valkind(args.main) != kGAB_BLOCK)
    return nullptr;

  struct gab_obj_block *b = GAB_VAL_TO_BLOCK(args.main);
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

  uint8_t *ip = p->src->bytecode.data + p->offset;
  uint8_t op = *ip++;

  return handlers[op](gab, ip, p->src->constants.data, b, gab.vm->sb,
                      gab.vm->sp);
}

#define IMPL_SEND_BLOCK(PREFIX)                                                \
  CASE_CODE(PREFIX##SEND_BLOCK) {                                              \
    gab_value *ks = READ_CONSTANTS;                                            \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    gab_value r = PEEK_N(have);                                                \
    gab_value m = ks[GAB_SEND_KMESSAGE];                                       \
                                                                               \
    if (__gab_unlikely(                                                        \
            !gab_valeq(GAB_VAL_TO_MESSAGE(m)->specs, ks[GAB_SEND_KSPECS])))    \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    if (__gab_unlikely(!gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE])))            \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC];                      \
                                                                               \
    PREFIX##CALL_BLOCK(b, have);                                               \
  }

#define IMPL_SEND_NATIVE(PREFIX)                                               \
  CASE_CODE(PREFIX##SEND_NATIVE) {                                             \
    gab_value *ks = READ_CONSTANTS;                                            \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    gab_value r = PEEK_N(have);                                                \
    gab_value m = ks[GAB_SEND_KMESSAGE];                                       \
                                                                               \
    if (__gab_unlikely(                                                        \
            !gab_valeq(GAB_VAL_TO_MESSAGE(m)->specs, ks[GAB_SEND_KSPECS])))    \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    if (__gab_unlikely(!gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE])))            \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    struct gab_obj_native *n = (void *)ks[GAB_SEND_KSPEC];                     \
                                                                               \
    PREFIX##CALL_NATIVE(n, have, true);                                        \
  }

#define IMPL_SEND_PRIMITIVE_CALL_BLOCK(PREFIX)                                 \
  CASE_CODE(PREFIX##SEND_PRIMITIVE_CALL_BLOCK) {                               \
    SKIP_SHORT;                                                                \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    gab_value r = PEEK_N(have);                                                \
                                                                               \
    if (__gab_unlikely(gab_valkind(r) != kGAB_BLOCK))                          \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(r);                           \
                                                                               \
    PREFIX##CALL_BLOCK(blk, have);                                             \
  }

#define IMPL_SEND_PRIMITIVE_CALL_NATIVE(PREFIX)                                \
  CASE_CODE(PREFIX##SEND_PRIMITIVE_CALL_NATIVE) {                              \
    SKIP_SHORT;                                                                \
    uint64_t have = compute_arity(VAR(), READ_BYTE);                           \
                                                                               \
    gab_value r = PEEK_N(have);                                                \
                                                                               \
    if (__gab_unlikely(gab_valkind(r) != kGAB_NATIVE))                         \
      MISS_CACHED_SEND();                                                      \
                                                                               \
    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(r);                           \
                                                                               \
    PREFIX##CALL_NATIVE(n, have, false);                                       \
  }

CASE_CODE(MATCHTAILSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];
  gab_value t = gab_valtype(EG(), r);

  if (__gab_unlikely(
          !gab_valeq(GAB_VAL_TO_MESSAGE(m)->specs, ks[GAB_SEND_KSPECS])))
    MISS_CACHED_SEND();

  uint8_t idx = GAB_SEND_HASH(t) * GAB_SEND_CACHE_SIZE;

  // TODO: Handle undefined and record case
  if (__gab_unlikely(ks[GAB_SEND_KTYPE + idx] != t))
    MISS_CACHED_SEND();

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC + idx];

  gab_value *from = SP() - have;
  gab_value *to = FB();

  memcpy(to, from, have * sizeof(gab_value));

  IP() = (void *)ks[GAB_SEND_KOFFSET + idx];
  BLOCK() = b;
  SP() = to + have;

  SET_VAR(have);

  NEXT();
}

CASE_CODE(MATCHSEND_BLOCK) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];
  gab_value t = gab_valtype(EG(), r);

  if (__gab_unlikely(
          !gab_valeq(GAB_VAL_TO_MESSAGE(m)->specs, ks[GAB_SEND_KSPECS])))
    MISS_CACHED_SEND();

  uint8_t idx = GAB_SEND_HASH(t) * GAB_SEND_CACHE_SIZE;

  // TODO: Handle undefined and record case
  if (__gab_unlikely(ks[GAB_SEND_KTYPE + idx] != t))
    MISS_CACHED_SEND();

  struct gab_obj_block *b = (void *)ks[GAB_SEND_KSPEC + idx];

  PUSH_FRAME();

  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);

  if (__gab_unlikely(!has_callspace(VM(), p->nslots - have)))
    ERROR(GAB_OVERFLOW, "");

  IP() = (void *)ks[GAB_SEND_KOFFSET + idx];
  BLOCK() = b;
  FB() = SP() - have;

  SET_VAR(have);

  NEXT();
}

static inline bool try_setup_localmatch(gab_value m, gab_value *ks,
                                        struct gab_obj_prototype *p) {
  gab_value specs = gab_msgrec(m);

  if (gab_reclen(specs) > 4 || gab_reclen(specs) < 2)
    return false;

  for (size_t i = 0; i < gab_reclen(specs); i++) {
    gab_value spec = gab_urecat(specs, i);

    if (gab_valkind(spec) != kGAB_BLOCK)
      return false;

    struct gab_obj_block *b = GAB_VAL_TO_BLOCK(spec);
    struct gab_obj_prototype *spec_p = GAB_VAL_TO_PROTOTYPE(b->p);

    if (spec_p->src != p->src)
      return false;

    gab_value t = gab_ushpat(gab_recshp(specs), i);

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

IMPL_SEND_NATIVE()
IMPL_SEND_BLOCK()
IMPL_SEND_BLOCK(TAIL)
IMPL_SEND_BLOCK(LOCAL)
IMPL_SEND_BLOCK(LOCALTAIL)
IMPL_SEND_PRIMITIVE_CALL_NATIVE()
IMPL_SEND_PRIMITIVE_CALL_BLOCK()
IMPL_SEND_PRIMITIVE_CALL_BLOCK(TAIL)
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

  if (__gab_unlikely(GAB_VAL_TO_MESSAGE(m)->specs != ks[GAB_SEND_KSPECS]))
    MISS_CACHED_SEND();

  if (__gab_unlikely(!gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE])))
    MISS_CACHED_SEND();

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
  SKIP_SHORT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  if (__gab_unlikely(gab_valkind(PEEK_N(have)) != kGAB_STRING))
    MISS_CACHED_SEND();

  if (__gab_unlikely(have < 2))
    PUSH(gab_nil), have++;

  if (__gab_unlikely(gab_valkind(PEEK_N(have - 1)) != kGAB_STRING &&
                     gab_valkind(PEEK_N(have - 1)) != kGAB_SIGIL)) {
    STORE_PRIMITIVE_ERROR_FRAME(1);
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, PEEK_N(have - 1),
          gab_valtype(EG(), PEEK_N(have - 1)), gab_valtype(EG(), PEEK_N(have)));
  }

  STORE_SP();

  gab_value val_a = PEEK_N(have);
  gab_value val_b = PEEK_N(have - 1);
  gab_value val_ab = gab_strcat(GAB(), val_a, val_b);

  DROP_N(have);
  PUSH(val_ab);

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SPLAT) {
  SKIP_SHORT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  if (__gab_unlikely(gab_valkind(r) != kGAB_RECORD))
    MISS_CACHED_SEND();

  struct gab_obj_record *rec = GAB_VAL_TO_RECORD(r);

  DROP_N(have);

  memcpy(SP(), rec->data, rec->len * sizeof(gab_value));
  SP() += rec->len;

  SET_VAR(rec->len);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_CALL_RECORD) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  if (__gab_unlikely(!gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE])))
    MISS_CACHED_SEND();

  if (__gab_unlikely(have < 2))
    PUSH(gab_nil), have++;

  gab_value k = PEEK_N(have - 1);

  switch (have) {
  case 2: {
    /* This is a normal property access - load the value from the record. */
    gab_value value = gab_recat(r, k);
    DROP_N(have);
    PUSH(value == gab_undefined ? gab_nil : value);
    break;
  }

  default:
    /* This is a property assignment, but we have extra values. */
    DROP_N(have - 3);
  case 3: {
    /* This is a property assignment, with the one value*/
    gab_value value = PEEK();

    DROP_N(3);

    gab_recput(GAB(), r, k, value);

    PUSH(r);

    break;
  }
  }

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SET) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);

  if (__gab_unlikely(!gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE])))
    MISS_CACHED_SEND();

  if (__gab_unlikely(have < 2))
    PUSH(gab_nil), have++;

  STORE_SP();

  gab_value res = gab_recput(GAB(), r, PEEK_N(have - 1), PEEK_N(have - 2));

  DROP_N(have);

  PUSH(res == gab_undefined ? gab_none : gab_ok);
  PUSH(res == gab_undefined ? gab_nil : res);

  SET_VAR(2);

  NEXT();
}

CASE_CODE(SEND_CONSTANT) {
  gab_value *ks = READ_CONSTANTS;
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  if (__gab_unlikely(
          !gab_valeq(GAB_VAL_TO_MESSAGE(m)->specs, ks[GAB_SEND_KSPECS])))
    MISS_CACHED_SEND();

  if (__gab_unlikely(ks[GAB_SEND_KTYPE] != gab_valtype(EG(), r)))
    MISS_CACHED_SEND();

  DROP_N(have);

  PUSH(ks[GAB_SEND_KSPEC]);

  SET_VAR(1);

  NEXT();
}

CASE_CODE(SEND_PROPERTY) {
  gab_value *ks = READ_CONSTANTS;
  uint8_t have_byte = READ_BYTE;
  uint64_t have = compute_arity(VAR(), have_byte);

  gab_value r = PEEK_N(have);
  gab_value m = ks[GAB_SEND_KMESSAGE];

  if (__gab_unlikely(
          !gab_valeq(GAB_VAL_TO_MESSAGE(m)->specs, ks[GAB_SEND_KSPECS])))
    MISS_CACHED_SEND();

  if (__gab_unlikely(!gab_egvalisa(EG(), r, ks[GAB_SEND_KTYPE])))
    MISS_CACHED_SEND();

  gab_value spec = gab_urecat(r, ks[GAB_SEND_KOFFSET]);

  switch (gab_valkind(spec)) {
  case kGAB_PRIMITIVE: {
    uint8_t op = gab_valtop(spec);

    IP() -= SEND_CACHE_DIST - 1;

    DISPATCH(op);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

    if (have_byte & fHAVE_TAIL)
      TAILCALL_BLOCK(blk, have);
    else
      CALL_BLOCK(blk, have);
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(spec);

    CALL_NATIVE(n, have, true);
  }
  default: {
    switch (have) {
    case 1:
      /* This is a normal property access - load the value from the record. */
      PEEK() = spec;
      break;

    default:
      /* This is a property assignment, but we have extra values. */
      DROP_N(have - 1);
    case 2: {
      /* This is a property assignment, with the one value*/
      gab_value value = POP();

      gab_urecput(GAB(), r, ks[GAB_SEND_KOFFSET], value);

      PEEK() = value;

      break;
    }
    }

    SET_VAR(1);

    NEXT();
  }
  }
}

CASE_CODE(RETURN) {
  uint64_t have = compute_arity(VAR(), READ_BYTE);

  gab_value *from = SP() - have;
  gab_value *to = FB();

  memmove(to, from, have * sizeof(gab_value));
  SP() = to + have;

  if (__gab_unlikely(VM()->fp < VM()->fb))
    return STORE_SP(), SET_VAR(have), ok(DISPATCH_ARGS());

  LOAD_FRAME();

  POP_FRAME();

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

  /*
   * When len and above are 1, we copy nonsense from the stack
   * prefer memmove to memcpy, as there might be overlap.
   */
  memmove(ap - len + 1, ap, above * sizeof(gab_value));

  PEEK_N(above + 1) = rec;

  /*
   * When packing at the entrypoint of a function
   * It is possible that we pack SP() to actually be lower than the
   * number of locals the function is expected to have. Move SP()
   * to past the locals section in this case.
   */
  while (SP() - FB() < BLOCK_PROTO()->nlocals)
    PUSH(gab_nil);

  SET_VAR(want + 1);

  NEXT();
}

CASE_CODE(RECORD) {
  uint8_t len = READ_BYTE;

  gab_gclock(GC());

  gab_value shape = gab_shape(GAB(), 2, len, SP() - len * 2);

  gab_value rec = gab_recordof(GAB(), shape, 2, SP() + 1 - (len * 2));

  DROP_N(len * 2);

  PUSH(rec);

  STORE_SP();

  gab_gcunlock(GC());

  NEXT();
}

CASE_CODE(TUPLE) {
  uint64_t len = compute_arity(VAR(), READ_BYTE);

  gab_gclock(GC());

  gab_value shape = gab_nshape(GAB(), len);

  gab_value rec = gab_recordof(GAB(), shape, 1, SP() - len);

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

  gab_value m = ks[GAB_SEND_KMESSAGE];
  gab_value r = PEEK_N(have);

  if (try_setup_localmatch(m, ks, BLOCK_PROTO())) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_MATCHSEND_BLOCK + adjust);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  /* Do the expensive lookup */
  struct gab_egimpl_rest res = gab_egimpl(EG(), m, r);

  if (__gab_unlikely(!res.status)) {
    PUSH_FRAME();
    ERROR(GAB_IMPLEMENTATION_MISSING, FMT_MISSINGIMPL, m, r,
          gab_valtype(EG(), r));
  }

  gab_value spec = res.status == kGAB_IMPL_PROPERTY
                       ? gab_primitive(OP_SEND_PROPERTY)
                       : gab_umsgat(m, res.offset);

  ks[GAB_SEND_KSPECS] = GAB_VAL_TO_MESSAGE(m)->specs;
  ks[GAB_SEND_KTYPE] = gab_valtype(EG(), r);
  ks[GAB_SEND_KOFFSET] = res.offset;

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

CASE_CODE(SEND_PRIMITIVE_CALL_MESSAGE) {
  gab_value *ks = READ_CONSTANTS;
  uint8_t have_byte = READ_BYTE;
  uint64_t have = compute_arity(VAR(), have_byte);

  gab_value m = PEEK_N(have);
  gab_value r = PEEK_N(have - 1);
  gab_value t = gab_valtype(EG(), r);

  if (__gab_unlikely(gab_valkind(m) != kGAB_MESSAGE))
    MISS_CACHED_SEND();

  struct gab_egimpl_rest res = gab_egimpl(EG(), m, r);

  if (__gab_unlikely(!res.status)) {
    PUSH_FRAME();
    ERROR(GAB_IMPLEMENTATION_MISSING, FMT_MISSINGIMPL, m, r,
          gab_valtype(EG(), r));
  }

  memmove(SP() - have, SP() - (have - 1), (have - 1) * sizeof(gab_value));

  gab_value spec = res.status == kGAB_IMPL_PROPERTY
                       ? gab_primitive(OP_SEND_PROPERTY)
                       : gab_umsgat(m, res.offset);

  ks[GAB_SEND_KOFFSET] = res.offset;
  ks[GAB_SEND_KTYPE] = t;

  switch (gab_valkind(spec)) {
  case kGAB_PRIMITIVE: {
    PEEK() = gab_nil;
    uint8_t op = gab_valtop(spec);

    IP() -= SEND_CACHE_DIST - 1;

    DISPATCH(op);
  }
  case kGAB_BLOCK: {
    struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

    have--;
    DROP();

    if (have_byte & fHAVE_TAIL)
      TAILCALL_BLOCK(blk, have);
    else
      CALL_BLOCK(blk, have);
  }
  case kGAB_NATIVE: {
    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(spec);

    have--;
    DROP();

    CALL_NATIVE(n, have, true);
  }
  default: {
    DROP_N(have);
    PUSH(spec);

    SET_VAR(1);

    NEXT();
  }
  }
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef IMPL_SEND_BINARY_PRIMITIVE
