#include "core.h"
#include "gab.h"

#define STATUS_NAMES
#include "engine.h"

#include "colors.h"
#include "lexer.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OP_HANDLER_ARGS                                                        \
  struct gab_triple gab, gab_value *kb, gab_value *fb, gab_value *sp,          \
      uint8_t *ip

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

#define DISPATCH_ARGS() GAB(), KB(), FB(), SP(), IP()

#define DISPATCH(op)                                                           \
  ({                                                                           \
    uint8_t o = (op);                                                          \
    LOG(o)                                                                     \
    return handlers[o](DISPATCH_ARGS());                                       \
  })

#define NEXT() DISPATCH(*IP()++);

#define ERROR(status, help, ...)                                               \
  return vm_error(GAB(), status, help __VA_OPT__(, ) __VA_ARGS__);

/*
  Lots of helper macros.
*/
#define GAB() (gab)
#define EG() (GAB().eg)
#define GC() (GAB().gc)
#define VM() (GAB().vm)
#define INSTR() (instr)
#define FRAME() (VM()->fp)
#define BLOCK() (FRAME()->b)
#define BLOCK_PROTO() (GAB_VAL_TO_PROTOTYPE(BLOCK()->p))
#define IP() (ip)
#define SP() (sp)
#define VAR() (*SP())
#define FB() (fb)
#define KB() (kb)
#define LOCAL(i) (FB()[i])
#define UPVALUE(i) (FRAME()->b->upvalues[i])
#define MOD_CONSTANT(k) (kb[k])

#if cGAB_DEBUG_VM
#define PUSH(value)                                                            \
  ({                                                                           \
    if (SP() > (FB() + BLOCK_PROTO()->as.block.nslots + 1)) {                  \
      fprintf(stderr,                                                          \
              "Stack exceeded frame "                                          \
              "(%d). %lu passed\n",                                            \
              BLOCK_PROTO()->as.block.nslots,                                  \
              SP() - FB() - BLOCK_PROTO()->as.block.nslots);                   \
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
#define PEEK_N(n) (*(SP() - (n)))

#define WRITE_BYTE(dist, n) (*(IP() - dist) = (n))

#define WRITE_INLINEBYTE(n) (*IP()++ = (n))
#define WRITE_INLINESHORT(n)                                                   \
  (IP() += 2, IP()[-2] = (n >> 8) & 0xFF, IP()[-1] = n & 0xFF)
#define WRITE_INLINEQWORD(n) (IP() += 8, *((uint64_t *)(IP() - 8)) = n)

#define SKIP_BYTE (IP()++)
#define SKIP_SHORT (IP() += 2)
#define SKIP_QWORD (IP() += 8)

#define READ_BYTE (*IP()++)
#define READ_SHORT (IP() += 2, (((uint16_t)IP()[-2] << 8) | IP()[-1]))
#define READ_QWORD (IP() += 8, (uint64_t *)(IP() - 8))

#define READ_CONSTANT (MOD_CONSTANT(READ_SHORT))

#define BINARY_PRIMITIVE(value_type, operation_type, operation)                \
  gab_value m = READ_CONSTANT;                                                 \
  SKIP_BYTE;                                                                   \
  SKIP_BYTE;                                                                   \
  SKIP_QWORD;                                                                  \
  SKIP_QWORD;                                                                  \
  SKIP_QWORD;                                                                  \
  if (__gab_unlikely(!__gab_valisn(PEEK2()))) {                                \
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);                                  \
    IP() -= SEND_CACHE_DIST;                                                   \
    NEXT();                                                                    \
  }                                                                            \
  if (__gab_unlikely(!__gab_valisn(PEEK()))) {                                 \
    STORE_PRIMITIVE_ERROR_FRAME(m, 1);                                         \
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, PEEK(),                         \
          gab_valtype(EG(), PEEK()), gab_valtype(EG(), PEEK2()));              \
  }                                                                            \
  operation_type b = gab_valton(POP());                                        \
  operation_type a = gab_valton(POP());                                        \
  PUSH(value_type(a operation b));                                             \
  VAR() = 1;

#define STORE_FRAME()                                                          \
  ({                                                                           \
    VM()->sp = SP();                                                           \
    VM()->fp->ip = IP();                                                       \
  })

#define STORE_ERROR_FRAME(m, have)                                             \
  ({                                                                           \
    VM()->fp++;                                                                \
    VM()->fp->m = m;                                                           \
    VM()->fp->b = NULL;                                                        \
    VM()->fp->slots = SP() - have;                                             \
  })

#define DROP_ERROR_FRAME() ({ VM()->fp--; })

#define STORE_PRIMITIVE_ERROR_FRAME(m, have)                                   \
  ({                                                                           \
    VM()->sp = SP();                                                           \
    VM()->fp->ip = IP();                                                       \
    STORE_ERROR_FRAME(m, have);                                                \
  })

#define LOAD_FRAME()                                                           \
  ({                                                                           \
    SP() = VM()->sp;                                                           \
    FB() = VM()->fp->slots;                                                    \
    IP() = VM()->fp->ip;                                                       \
    KB() = BLOCK_PROTO()->src->constants.data;                                 \
  })

#define SEND_CACHE_DIST 29

static inline size_t compute_token_from_ip(struct gab_vm_frame *f) {
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(f->b->p);

  size_t offset = f->ip - p->src->bytecode.data - 1;

  size_t token = v_uint64_t_val_at(&p->src->bytecode_toks, offset);

  return token;
}

/**
 * TODO: Introduce a way for native's to be on the stack here.
 * This way, stack traces won't look weird
 *
 */

struct gab_err_argt vm_frame_build_err(struct gab_vm_frame *fp,
                                       enum gab_status s, const char *fmt) {
  if (fp->b) {
    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(fp->b->p);

    size_t tok = compute_token_from_ip(fp);

    return (struct gab_err_argt){
        .tok = tok,
        .message = fp->m,
        .src = p->src,
        .note_fmt = fmt,
        .status = s,
    };
  }

  return (struct gab_err_argt){
      .message = fp->m,
      .note_fmt = fmt,
      .status = s,
  };
}

a_gab_value *vvm_error(struct gab_triple gab, enum gab_status s,
                       const char *fmt, va_list va) {
  struct gab_vm_frame *f = gab.vm->fb + 1;

  struct gab_triple dont_exit = gab;
  dont_exit.flags &= ~fGAB_EXIT_ON_PANIC;

  while (f != gab.vm->fp) {
    gab_fvpanic(dont_exit, stderr, NULL, vm_frame_build_err(f, GAB_NONE, ""));
    f++;
  }

  gab_fvpanic(gab, stderr, va, vm_frame_build_err(f, s, fmt));

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
  "found:\n\n | $\n\nof type:\n\n | $\n\nbut expected type:\n\n | $"

#define FMT_MISSINGIMPL "$ does not specialize for:\n\n | $\n\nof type:\n\n | $"

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
  self->fp = self->fb;
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
      "block",
      "line",
  };

  gab_value line = gab_nil;

  if (f->b) {
    struct gab_src *src = GAB_VAL_TO_PROTOTYPE(f->b->p)->src;
    size_t tok = compute_token_from_ip(f);
    line = gab_number(v_uint64_t_val_at(&src->token_lines, tok));
  }

  gab_value values[] = {
      f->m,
      line,
  };

  size_t len = sizeof(keys) / sizeof(keys[0]);

  return gab_srecord(gab, len, keys, values);
}

void gab_fvminspect(FILE *stream, struct gab_vm *vm, uint64_t value) {
  uint64_t frame_count = vm->fp - vm->fb;

  if (value >= frame_count)
    return;

  struct gab_vm_frame *f = vm->fp - value;

  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(f->b->p);

  struct gab_obj_string *func_name = GAB_VAL_TO_STRING(p->name);

  fprintf(stream,
          ANSI_COLOR_GREEN " %03lu" ANSI_COLOR_RESET " closure:" ANSI_COLOR_CYAN
                           "%-20.*s" ANSI_COLOR_RESET " %d upvalues\n",
          frame_count - value, (int32_t)func_name->len, func_name->data,
          p->as.block.nupvalues);

  int top = p->as.block.nslots;
  if (f->slots + top > vm->sp)
    top = vm->sp - f->slots;

  for (int32_t i = top; i >= 0; i--) {
    fprintf(stream, "%2s" ANSI_COLOR_YELLOW "%4i " ANSI_COLOR_RESET,
            vm->sp == f->slots + i ? "->" : "", i);
    gab_fvalinspect(stream, f->slots[i], 0);
    fprintf(stream, "\n");
  }
}

static inline int32_t compute_arity(size_t var, uint8_t have) {
  if (have & FLAG_VAR_EXP)
    return var + (have >> 1);
  else
    return have >> 1;
}

static inline uint64_t trim_values(gab_value *from, gab_value *to,
                                   uint64_t have, uint8_t want) {
  uint64_t nulls = 0;

  if (__gab_unlikely(((have != want) && (want != VAR_EXP)))) {
    if (have > want)
      have = want;
    else
      nulls = want - have;
  }

  const uint64_t got = have + nulls;

  while (have--)
    *to++ = *from++;

  while (nulls--)
    *to++ = gab_nil;

  return got;
}

static inline gab_value *trim_return(gab_value *from, gab_value *to,
                                     uint64_t have, uint8_t want) {
  uint64_t got = trim_values(from, to, have, want);

  gab_value *sp = to + got;
  *sp = got;

  return sp;
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

static inline bool call_suspense(struct gab_vm *vm,
                                 struct gab_obj_suspense *sus, uint8_t have,
                                 uint8_t want) {
  int32_t space_needed = sus->nslots;

  if (space_needed > 0 && !has_callspace(vm, space_needed))
    return false;

  struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(sus->p);

  vm->fp++;
  vm->fp->b = GAB_VAL_TO_BLOCK(sus->b);
  vm->fp->ip = proto->src->bytecode.data + proto->begin;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - have - 1;

  gab_value *from = vm->sp - have;
  gab_value *to = vm->fp->slots + sus->nslots;

  vm->sp = trim_return(from, to, have, proto->as.suspense.want);
  memcpy(vm->fp->slots, sus->slots, sus->nslots * sizeof(gab_value));

  return true;
}

size_t gab_vmpush(struct gab_vm *vm, gab_value value) {
  return gab_nvmpush(vm, 1, &value);
}

size_t gab_nvmpush(struct gab_vm *vm, uint64_t argc, gab_value argv[argc]) {
  if (!has_callspace(vm, argc)) {
    return -1;
  }

  for (uint8_t n = 0; n < argc; n++) {
    *vm->sp++ = argv[n];
  }

  return argc;
}

static inline bool call_block(struct gab_vm *vm, gab_value m,
                              struct gab_obj_block *b, uint64_t have,
                              uint8_t want) {
  struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(b->p);
  bool wants_var = p->as.block.narguments == VAR_EXP;
  size_t len = (wants_var ? have : p->as.block.narguments) + 1;

  if (__gab_unlikely(!has_callspace(vm, p->as.block.nslots - len - 1)))
    return false;

  vm->fp++;
  vm->fp->m = m;
  vm->fp->b = b;
  vm->fp->ip = p->src->bytecode.data + p->begin;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - have - 1;

  // Update the SP to point just past the locals section
  // Or past the arguments if we're using VAR_EXP
  size_t offset = (wants_var ? len : p->as.block.nlocals);

  // Trim arguments into the slots
  vm->sp = trim_return(vm->fp->slots + 1, vm->fp->slots + 1, have, offset - 1);

  return true;
}

// Maybe change these to do tailcalls?
static inline a_gab_value *call_native(struct gab_triple gab,
                                       struct gab_obj_native *b, uint8_t arity,
                                       uint8_t want, bool is_message) {
  gab_value *to = gab.vm->sp - arity - 1;

  gab_value *before = gab.vm->sp;

  // Only pass in the extra "self" argument
  // if this is a message.
  a_gab_value *res =
      (*b->function)(gab, arity + is_message, gab.vm->sp - arity - is_message);

  uint64_t have = gab.vm->sp - before;

  // Always have atleast one result
  if (have == 0)
    *gab.vm->sp++ = gab_nil, have++;

  // Trim our return values into our destination slot
  gab.vm->sp = trim_return(gab.vm->sp - have, to, have, want);

  return res;
}

a_gab_value *gab_run(struct gab_triple gab, struct gab_run_argt args) {
  gab.flags = args.flags;

  /*
   ----------- BEGIN RUN BODY -----------
  */
  VM() = NEW(struct gab_vm);
  gab_vmcreate(VM(), args.len, args.argv);

  uint8_t *ip = NULL;
  gab_value *sp = NULL, *kb = NULL, *fb = NULL;

  *VM()->sp++ = args.main;
  for (uint8_t i = 0; i < args.len; i++)
    *VM()->sp++ = args.argv[i];

  switch (gab_valkind(args.main)) {
  case kGAB_BLOCK: {
    struct gab_obj_block *b = GAB_VAL_TO_BLOCK(args.main);
    if (__gab_unlikely(!call_block(VM(), gab_nil, b, args.len, VAR_EXP)))
      ERROR(GAB_OVERFLOW, "");
    break;
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *s = GAB_VAL_TO_SUSPENSE(args.main);
    if (__gab_unlikely(!call_suspense(VM(), s, args.len, VAR_EXP)))
      ERROR(GAB_OVERFLOW, "");
    break;
  }
  default:
    return a_gab_value_one(gab_nil);
  }

  LOAD_FRAME();

  uint8_t op = *IP()++;
  a_gab_value *res = handlers[op](GAB(), KB(), FB(), SP(), IP());
  return res;
}

CASE_CODE(SEND_ANA) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  SKIP_BYTE;

  gab_value r = PEEK_N(have + 1);
  gab_value t = gab_valtype(EG(), r);

  /* Do the expensive lookup */
  struct gab_egimpl_rest res = gab_egimpl(gab.eg, m, r);

  if (__gab_unlikely(!res.status)) {
    STORE_FRAME();
    ERROR(GAB_IMPLEMENTATION_MISSING, FMT_MISSINGIMPL, m, r,
          gab_valtype(EG(), r));
  }

  gab_value spec = res.status == sGAB_IMPL_PROPERTY
                       ? gab_primitive(OP_SEND_MONO_PROPERTY)
                       : gab_umsgat(m, res.offset);

  WRITE_INLINEQWORD(GAB_VAL_TO_MESSAGE(m)->specs);
  WRITE_INLINEQWORD(t);
  WRITE_INLINEQWORD(res.status == sGAB_IMPL_PROPERTY ? res.offset : spec);

  switch (gab_valkind(spec)) {
  case kGAB_PRIMITIVE:
    WRITE_BYTE(SEND_CACHE_DIST, gab_valtop(spec));
    break;
  case kGAB_BLOCK:
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_MONO_BLOCK);
    break;
  case kGAB_NATIVE:
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_MONO_NATIVE);
    break;
  default:
    STORE_FRAME();
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, spec, gab_type(EG(), kGAB_BLOCK),
          res.type);
  }

  IP() -= SEND_CACHE_DIST;
  NEXT();
}

CASE_CODE(SEND_MONO_BLOCK) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  gab_value spec = *READ_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

  STORE_FRAME();

  if (__gab_unlikely(!call_block(VM(), m, blk, have, want)))
    ERROR(GAB_OVERFLOW, "");

  LOAD_FRAME();

  NEXT();
}

a_gab_value *return_err(OP_HANDLER_ARGS) {
  uint64_t have = *gab.vm->sp;
  gab_value *from = gab.vm->sp - have;

  a_gab_value *results = a_gab_value_empty(have);
  memcpy(results->data, from, have * sizeof(gab_value));

  gab_niref(GAB(), 1, results->len, results->data);

  return results;
}

a_gab_value *return_ok(OP_HANDLER_ARGS) {
  uint64_t have = *gab.vm->sp;
  gab_value *from = gab.vm->sp - have;

  a_gab_value *results = a_gab_value_empty(have + 1);
  results->data[0] = gab_string(gab, "ok");
  memcpy(results->data + 1, from, have * sizeof(gab_value));

  gab_niref(GAB(), 1, results->len, results->data);

  VM()->sp = VM()->sb;

  gab_vmdestroy(EG(), VM());
  DESTROY(VM());

  return results;
}

CASE_CODE(SEND_MONO_NATIVE) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  gab_value spec = *READ_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  STORE_FRAME();

  STORE_ERROR_FRAME(m, have);

  a_gab_value *res =
      call_native(GAB(), GAB_VAL_TO_NATIVE(spec), have, want, true);

  if (__gab_unlikely(res))
    return res;

  DROP_ERROR_FRAME();

  LOAD_FRAME();

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_CALL_NATIVE) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  SKIP_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  STORE_FRAME();

  STORE_ERROR_FRAME(m, have);

  a_gab_value *res =
      call_native(GAB(), GAB_VAL_TO_NATIVE(r), have, want, false);

  if (__gab_unlikely(res))
    return res;

  DROP_ERROR_FRAME();

  LOAD_FRAME();

  NEXT();
}

CASE_CODE(SEND_DYN) {
  SKIP_SHORT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  SKIP_QWORD;
  SKIP_QWORD;
  SKIP_QWORD;

  gab_value r = PEEK_N(have + 2);
  gab_value m = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valkind(m) != kGAB_MESSAGE)) {
    STORE_FRAME();
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, m, gab_valtype(EG(), m),
          gab_type(EG(), kGAB_MESSAGE));
  }

  struct gab_egimpl_rest res = gab_egimpl(gab.eg, m, r);

  if (__gab_unlikely(!res.status)) {
    STORE_FRAME();
    ERROR(GAB_IMPLEMENTATION_MISSING, FMT_MISSINGIMPL, m, r,
          gab_valtype(EG(), r));
  }

  gab_value spec = res.status == sGAB_IMPL_PROPERTY
                       ? gab_primitive(OP_SEND_MONO_PROPERTY)
                       : gab_umsgat(m, res.offset);

  switch (gab_valkind(spec)) {
  case kGAB_BLOCK: {
    // Shift our args down and forget about the message being called
    SP() = trim_return(SP() - have, SP() - (have + 1), have, have);

    struct gab_obj_block *b = GAB_VAL_TO_BLOCK(spec);

    STORE_FRAME();

    if (__gab_unlikely(!call_block(VM(), m, b, have, want)))
      ERROR(GAB_OVERFLOW, "");

    LOAD_FRAME();

    NEXT();
  }
  case kGAB_NATIVE: {
    // Shift our args down and forget about the message being called
    SP() = trim_return(SP() - have, SP() - (have + 1), have, have);

    struct gab_obj_native *n = GAB_VAL_TO_NATIVE(spec);

    STORE_FRAME();

    STORE_ERROR_FRAME(m, have);

    a_gab_value *res = call_native(GAB(), n, have, want, true);

    if (__gab_unlikely(res))
      return res;

    DROP_ERROR_FRAME();

    LOAD_FRAME();

    NEXT();
  }
  case kGAB_PRIMITIVE: {
    // We're set up to dispatch to the primitive. Don't do any code
    // modification, though. We don't actually want to change the
    // state this instruction is in.
    // Dispatch to one-past the beginning of this instruction
    // we want to pretend we already read the op-code.
    // TODO: Handle primitive calls with the wrong number of
    // arguments
    uint8_t op = gab_valtop(spec);

    IP() -= SEND_CACHE_DIST - 1;
    want = op >= OP_SEND_PRIMITIVE_CALL_NATIVE ? VAR_EXP : 1;
    // Shift our args down and forget about the message being called
    SP() = trim_return(SP() - have, SP() - (have + 1), have, want);

    DISPATCH(op);
  }
  default:
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, spec, gab_valtype(EG(), r));
  }
}

CASE_CODE(SEND_PRIMITIVE_CALL_BLOCK) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  SKIP_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(r);

  STORE_FRAME();

  if (__gab_unlikely(!call_block(VM(), m, blk, have, want)))
    ERROR(GAB_OVERFLOW, "");

  LOAD_FRAME();

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_CALL_SUSPENSE) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  SKIP_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  STORE_FRAME();

  if (__gab_unlikely(!call_suspense(VM(), GAB_VAL_TO_SUSPENSE(r), have, want)))
    ERROR(GAB_OVERFLOW, "");

  LOAD_FRAME();

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_ADD) {
  BINARY_PRIMITIVE(gab_number, double, +);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SUB) {
  BINARY_PRIMITIVE(gab_number, double, -);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_MUL) {
  BINARY_PRIMITIVE(gab_number, double, *);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_DIV) {
  BINARY_PRIMITIVE(gab_number, double, /);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_MOD) {
  BINARY_PRIMITIVE(gab_number, uint64_t, %);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_BOR) {
  BINARY_PRIMITIVE(gab_number, uint64_t, |);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_BND) {
  BINARY_PRIMITIVE(gab_number, uint64_t, &);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_LSH) {
  BINARY_PRIMITIVE(gab_number, uint64_t, <<);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_RSH) {
  BINARY_PRIMITIVE(gab_number, uint64_t, >>);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_LT) {
  BINARY_PRIMITIVE(gab_bool, double, <);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_LTE) {
  BINARY_PRIMITIVE(gab_bool, double, <=);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_GT) {
  BINARY_PRIMITIVE(gab_bool, double, >);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_GTE) {
  BINARY_PRIMITIVE(gab_bool, double, >=);
  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_CONCAT) {
  gab_value m = READ_CONSTANT;
  SKIP_BYTE;
  SKIP_BYTE;
  SKIP_QWORD;
  SKIP_QWORD;
  SKIP_QWORD;

  if (__gab_unlikely(gab_valkind(PEEK2()) != kGAB_STRING)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(gab_valkind(PEEK()) != kGAB_STRING)) {
    STORE_PRIMITIVE_ERROR_FRAME(m, 1);
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, PEEK(),
          gab_valtype(EG(), PEEK()));
  }

  STORE_FRAME();

  gab_value b = POP();
  gab_value a = POP();
  gab_value ab = gab_strcat(GAB(), a, b);

  PUSH(ab);

  VAR() = 1;

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_EQ) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  SKIP_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  SKIP_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  gab_value b = POP();
  gab_value a = POP();

  PUSH(gab_bool(a == b));

  VAR() = 1;

  NEXT();
}

CASE_CODE(SEND_PRIMITIVE_SPLAT) {
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  uint8_t want = READ_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  SKIP_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  struct gab_obj_record *rec = GAB_VAL_TO_RECORD(r);

  DROP_N(have + 1);

  SP() = trim_return(rec->data, SP(), rec->len, want);

  NEXT();
}

CASE_CODE(SEND_MONO_PROPERTY) {
  // TODO: Break this into two opcodes
  gab_value m = READ_CONSTANT;
  uint64_t have = compute_arity(VAR(), READ_BYTE);
  SKIP_BYTE;
  gab_value cached_specs = *READ_QWORD;
  gab_value cached_type = *READ_QWORD;
  uint64_t prop_offset = *READ_QWORD;

  gab_value r = PEEK_N(have + 1);

  if (__gab_unlikely(gab_valtype(EG(), r) != cached_type)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  if (__gab_unlikely(cached_specs != GAB_VAL_TO_MESSAGE(m)->specs)) {
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
    IP() -= SEND_CACHE_DIST;
    NEXT();
  }

  switch (have) {
  case 0:
    /* Simply load the value into the top of the stack */
    PEEK() = gab_urecat(r, prop_offset);
    break;

  default:
    /* Drop all the values we don't need, then fallthrough */
    DROP_N(have - 1);

  case 1: {
    /* Pop the top value */
    gab_value value = POP();
    gab_urecput(GAB(), r, prop_offset, value);
    PEEK() = value;
    break;
  }
  }

  VAR() = 1;

  NEXT();
}

CASE_CODE(YIELD) {
  gab_value proto = READ_CONSTANT;
  uint8_t have = compute_arity(VAR(), READ_BYTE);

  uint64_t frame_len = SP() - FB() - have;

  STORE_FRAME();

  gab_value sus =
      gab_suspense(GAB(), __gab_obj(BLOCK()), proto, frame_len, FB());

  PUSH(sus);

  have++;

  gab_value *from = SP() - have;
  gab_value *to = FRAME()->slots;

  VM()->sp = trim_return(from, to, have, FRAME()->want);

  if (__gab_unlikely(VM()->fp - VM()->fb == 1))
    return return_ok(DISPATCH_ARGS());

  VM()->fp--;

  LOAD_FRAME();

  NEXT();

  NEXT();
}

CASE_CODE(RETURN) {
  uint8_t have = compute_arity(VAR(), READ_BYTE);

  gab_value *from = SP() - have;
  gab_value *to = FRAME()->slots;

  VM()->sp = trim_return(from, to, have, FRAME()->want);

  if (__gab_unlikely(VM()->fp - VM()->fb == 1))
    return return_ok(DISPATCH_ARGS());

  VM()->fp--;

  LOAD_FRAME();

  NEXT();
}

CASE_CODE(NOP) { NEXT(); }

CASE_CODE(CONSTANT) {
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

CASE_CODE(DUP) {
  gab_value a = PEEK();
  PUSH(a);
  NEXT();
}

CASE_CODE(SWAP) {
  gab_value tmp = PEEK();
  PEEK() = PEEK2();
  PEEK2() = tmp;
  NEXT();
}

CASE_CODE(INTERPOLATE) {
  uint8_t n = READ_BYTE;
  STORE_FRAME();
  gab_value str = gab_valintos(GAB(), PEEK_N(n));

  for (uint8_t i = n - 1; i > 0; i--) {
    gab_value curr = gab_valintos(GAB(), PEEK_N(i));
    str = gab_strcat(GAB(), str, curr);
  }

  POP_N(n);

  PUSH(str);

  NEXT();
}

CASE_CODE(LOGICAL_AND) {
  uint16_t offset = READ_SHORT;
  gab_value cond = PEEK();

  if (gab_valintob(cond))
    DROP();
  else
    ip += offset;

  NEXT();
}

CASE_CODE(LOGICAL_OR) {
  uint16_t offset = READ_SHORT;
  gab_value cond = PEEK();

  if (gab_valintob(cond))
    ip += offset;
  else
    DROP();

  NEXT();
}

CASE_CODE(PUSH_UNDEFINED) {
  PUSH(gab_undefined);
  NEXT();
}

CASE_CODE(PUSH_NIL) {
  PUSH(gab_nil);
  NEXT();
}

CASE_CODE(PUSH_TRUE) {
  PUSH(gab_bool(true));
  NEXT();
}

CASE_CODE(PUSH_FALSE) {
  PUSH(gab_bool(false));
  NEXT();
}

CASE_CODE(NEGATE) {
  if (__gab_unlikely(!__gab_valisn(PEEK()))) {
    STORE_FRAME();
    ERROR(GAB_TYPE_MISMATCH, FMT_TYPEMISMATCH, gab_valtype(EG(), PEEK()),
          PEEK());
  }

  gab_value new_value = gab_number(-gab_valton(POP()));

  PUSH(new_value);

  NEXT();
}

CASE_CODE(NOT) {
  gab_value new_value = gab_bool(!gab_valintob(POP()));
  PUSH(new_value);
  NEXT();
}

CASE_CODE(TYPE) {
  PEEK() = gab_valtype(EG(), PEEK());
  NEXT();
}

CASE_CODE(LOAD_LOCAL) {
  PUSH(LOCAL(READ_BYTE));
  NEXT();
}

CASE_CODE(STORE_LOCAL) {
  LOCAL(READ_BYTE) = PEEK();
  NEXT();
}

CASE_CODE(POP_STORE_LOCAL) {
  LOCAL(READ_BYTE) = POP();
  NEXT();
}

CASE_CODE(LOAD_UPVALUE) {
  PUSH(UPVALUE(READ_BYTE));
  NEXT();
}

CASE_CODE(POP_JUMP_IF_TRUE) {
  uint16_t dist = READ_SHORT;
  ip += dist * gab_valintob(POP());
  NEXT();
}

CASE_CODE(POP_JUMP_IF_FALSE) {
  uint16_t dist = READ_SHORT;
  ip += dist * !gab_valintob(POP());
  NEXT();
}

CASE_CODE(JUMP_IF_TRUE) {
  uint16_t dist = READ_SHORT;
  ip += dist * gab_valintob(PEEK());
  NEXT();
}

CASE_CODE(JUMP_IF_FALSE) {
  uint16_t dist = READ_SHORT;
  ip += dist * !gab_valintob(PEEK());
  NEXT();
}

CASE_CODE(JUMP) {
  uint16_t dist = READ_SHORT;
  ip += dist;
  NEXT();
}

CASE_CODE(LOOP) {
  uint16_t dist = READ_SHORT;
  ip -= dist;
  NEXT();
}

CASE_CODE(BLOCK) {
  gab_value p = READ_CONSTANT;

  STORE_FRAME();
  gab_value blk = gab_block(GAB(), p);

  struct gab_obj_block *b = GAB_VAL_TO_BLOCK(blk);
  struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(p);

  for (int i = 0; i < proto->as.block.nupvalues; i++) {
    uint8_t flags = proto->data[i * 2];
    uint8_t index = proto->data[i * 2 + 1];

    if (flags & fVAR_LOCAL) {
      b->upvalues[i] = LOCAL(index);
    } else {
      b->upvalues[i] = BLOCK()->upvalues[index];
    }
  }

  PUSH(blk);

  NEXT();
}

CASE_CODE(MESSAGE) {
  gab_value p = READ_CONSTANT;
  gab_value m = READ_CONSTANT;
  gab_value r = PEEK();

  STORE_FRAME();
  gab_value blk = gab_block(GAB(), p);

  struct gab_obj_block *b = GAB_VAL_TO_BLOCK(blk);
  struct gab_obj_prototype *proto = GAB_VAL_TO_PROTOTYPE(p);

  for (int i = 0; i < b->nupvalues; i++) {
    uint8_t flags = proto->data[i * 2];
    uint8_t index = proto->data[i * 2 + 1];

    if (flags & fVAR_LOCAL) {
      assert(index < BLOCK_PROTO()->as.block.nlocals);
      b->upvalues[i] = LOCAL(index);
    } else {
      assert(index < BLOCK()->nupvalues);
      b->upvalues[i] = BLOCK()->upvalues[index];
    }
  }

  if (__gab_unlikely(gab_msgput(GAB(), m, r, blk) == gab_undefined)) {
    STORE_FRAME();
    ERROR(GAB_IMPLEMENTATION_EXISTS,
          ANSI_COLOR_GREEN "$" ANSI_COLOR_RESET
                           " already specializes for type: " ANSI_COLOR_GREEN
                           "$" ANSI_COLOR_RESET,
          m, r);
  }

  PEEK() = m;

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

  STORE_FRAME();

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
  while (SP() - FB() < BLOCK_PROTO()->as.block.nlocals)
    PUSH(gab_nil);

  VAR() = want + 1;

  NEXT();
}

CASE_CODE(RECORD) {
  uint8_t len = READ_BYTE;

  gab_gclock(gab.gc);

  gab_value shape = gab_shape(GAB(), 2, len, SP() - len * 2);

  gab_value rec = gab_recordof(GAB(), shape, 2, SP() + 1 - (len * 2));

  DROP_N(len * 2);

  PUSH(rec);

  STORE_FRAME();

  gab_gcunlock(gab.gc);

  NEXT();
}

CASE_CODE(TUPLE) {
  uint64_t len = compute_arity(VAR(), READ_BYTE);

  gab_gclock(gab.gc);

  gab_value shape = gab_nshape(GAB(), len);

  gab_value rec = gab_recordof(GAB(), shape, 1, SP() - len);

  DROP_N(len);

  PUSH(rec);

  STORE_FRAME();

  gab_gcunlock(gab.gc);

  NEXT();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_PRIMITIVE
