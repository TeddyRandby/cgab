#include "include/vm.h"
#include "include/char.h"
#include "include/colors.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/module.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gab_vm_container_cb(void *data) { DESTROY(data); }

gab_value vm_error(struct gab_triple gab, uint8_t flags, enum gab_status e,
                   const char *help_fmt, ...) {

  va_list va;
  va_start(va, help_fmt);

  struct gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(gab.vm->fp->b->p);

  size_t offset = gab.vm->fp->ip - p->mod->bytecode.data - 1;

  gab_verr(
      (struct gab_err_argt){
          .tok = v_uint64_t_val_at(&p->mod->bytecode_toks, offset),
          .context = p->mod->name,
          .note_fmt = help_fmt,
          .flags = flags,
          .mod = p->mod,
          .status = e,
      },
      va);

  va_end(va);

  return gab_nil;
}

gab_value gab_vm_panic(struct gab_triple gab, const char *msg) {
  vm_error(gab, fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC, GAB_PANIC, msg);
  exit(1);
}

void gab_vmcreate(struct gab_vm *self, uint8_t flags, size_t argc,
                  gab_value argv[argc]) {
  self->fp = self->fb;
  self->sp = self->sb;
  self->fp->slots = self->sp;
  self->flags = flags;
}

void gab_vmdestroy(struct gab_eg *gab, struct gab_vm *self) {}

void gab_fpry(FILE *stream, struct gab_vm *vm, uint64_t value) {
  uint64_t frame_count = vm->fp - vm->fb;

  if (value >= frame_count)
    return;

  struct gab_vm_frame *f = vm->fp - value;

  struct gab_obj_block_proto *proto = GAB_VAL_TO_BLOCK_PROTO(f->b->p);

  struct gab_obj_string *func_name = GAB_VAL_TO_STRING(proto->mod->name);

  fprintf(stream,
          ANSI_COLOR_GREEN " %03lu" ANSI_COLOR_RESET " closure:" ANSI_COLOR_CYAN
                           "%-20.*s" ANSI_COLOR_RESET " %d upvalues\n",
          frame_count - value, (int32_t)func_name->len, func_name->data,
          proto->nupvalues);

  for (int32_t i = proto->nslots - 1; i >= 0; i--) {
    fprintf(stream, "%2s" ANSI_COLOR_YELLOW "%4i " ANSI_COLOR_RESET "%V\n",
            vm->sp == f->slots + i ? "->" : "", i, f->slots[i]);
  }
}

static inline int32_t parse_have(struct gab_vm *vm, uint8_t have) {
  if (have & FLAG_VAR_EXP)
    return *vm->sp + (have >> 1);
  else
    return have >> 1;
}

static inline uint64_t trim_values(gab_value *from, gab_value *to,
                                   uint64_t have, uint8_t want) {
  uint64_t nulls = 0;

  if ((have != want) && (want != VAR_EXP)) {
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

static inline bool has_callspace(struct gab_vm *vm, uint64_t space_needed) {
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
  int32_t space_needed = sus->len;

  if (space_needed > 0 && !has_callspace(vm, space_needed))
    return false;

  struct gab_obj_suspense_proto *proto = GAB_VAL_TO_SUSPENSE_PROTO(sus->p);

  vm->fp++;
  vm->fp->b = GAB_VAL_TO_BLOCK(sus->b);
  vm->fp->ip =
      GAB_VAL_TO_BLOCK_PROTO(vm->fp->b->p)->mod->bytecode.data + proto->offset;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - have - 1;

  gab_value *from = vm->sp - have;
  gab_value *to = vm->fp->slots + sus->len;

  vm->sp = trim_return(from, to, have, proto->want);
  memcpy(vm->fp->slots, sus->frame, sus->len * sizeof(gab_value));

  return true;
}

int32_t gab_vm_push(struct gab_vm *vm, uint64_t argc, gab_value argv[argc]) {
  if (!has_callspace(vm, argc)) {
    return -1;
  }

  for (uint8_t n = 0; n < argc; n++) {
    *vm->sp++ = argv[n];
  }

  return argc;
}

static inline bool call_block(struct gab_vm *vm, struct gab_obj_block *b,
                              uint64_t have, uint8_t want) {
  struct gab_obj_block_proto *proto = GAB_VAL_TO_BLOCK_PROTO(b->p);
  uint64_t len = proto->narguments == VAR_EXP ? have : proto->narguments;

  if (!has_callspace(vm, proto->nslots - len - 1))
    return false;

  vm->fp++;
  vm->fp->b = b;
  vm->fp->ip = proto->mod->bytecode.data;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - have - 1;
  
  // Update the SP to point just past the locals section
  vm->sp = vm->fp->slots + proto->nlocals;

  // Trim arguments into the slots
  *vm->sp = trim_values(vm->fp->slots + 1, vm->fp->slots + 1, have, len);

  return true;
}

static inline void call_builtin(struct gab_triple gab,
                                struct gab_obj_builtin *b, uint8_t arity,
                                uint8_t want, bool is_message) {
  gab_value *to = gab.vm->sp - arity - 1; // Is this -1 correct?

  gab_value *before = gab.vm->sp;

  // Only pass in the extra "self" argument
  // if this is a message.
  (*b->function)(gab, arity + is_message, gab.vm->sp - arity - is_message);

  uint8_t have = gab.vm->sp - before;

  // There is always an extra to trim bc of
  // the receiver or callee.
  gab.vm->sp = trim_return(gab.vm->sp - have, to, have, want);
}

a_gab_value *gab_vmrun(struct gab_triple gab, gab_value main, uint8_t flags,
                       size_t argc, gab_value argv[argc]) {
#if cGAB_LOG_VM
#define LOG() printf("OP_%s\n", gab_opcode_names[*(ip)])
#else
#define LOG()
#endif

  /*
    If we're using computed gotos, create
    the jump table and the dispatch
    instructions.
  */
  static const void *dispatch_table[256] = {
#define OP_CODE(name) &&code_##name,
#include "include/bytecode.h"
#undef OP_CODE
  };

#define CASE_CODE(name) code_##name
#define LOOP()
#define NEXT()                                                                 \
  ({                                                                           \
    LOG();                                                                     \
    goto *dispatch_table[(INSTR() = *IP()++)];                                 \
  })

#define BINARY_PRIMITIVE(value_type, operation_type, operation)                \
  SKIP_SHORT;                                                                  \
  SKIP_BYTE;                                                                   \
  SKIP_BYTE;                                                                   \
  SKIP_BYTE;                                                                   \
  SKIP_QWORD;                                                                  \
  SKIP_QWORD;                                                                  \
  if (!__gab_valisn(PEEK2())) {                                                \
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);                                  \
    IP() -= SEND_CACHE_DIST;                                                   \
    NEXT();                                                                    \
  }                                                                            \
  if (!__gab_valisn(PEEK())) {                                                 \
    STORE_FRAME();                                                             \
    return ERROR(GAB_NOT_NUMERIC, "Found %V %V", gab_valtyp(EG(), PEEK()),     \
                 PEEK());                                                      \
  }                                                                            \
  operation_type b = gab_valton(POP());                                        \
  operation_type a = gab_valton(POP());                                        \
  PUSH(value_type(a operation b));                                             \
  VAR() = 1;

/*
  Lots of helper macros.
*/
#define GAB() (gab)
#define EG() (GAB().eg)
#define GC() (GAB().gc)
#define VM() (GAB().vm)
#define INSTR() (instr)
#define FRAME() (VM()->fp)
#define CLOSURE() (FRAME()->b)
#define BLOCK_PROTO() (GAB_VAL_TO_BLOCK_PROTO(CLOSURE()->p))
#define MODULE() (BLOCK_PROTO()->mod)
#define IP() (ip)
#define TOP() (VM()->sp)
#define VAR() (*TOP())
#define SLOTS() (FRAME()->slots)
#define LOCAL(i) (SLOTS()[i])
#define UPVALUE(i) (CLOSURE()->upvalues[i])
#define MOD_CONSTANT(k) (v_gab_constant_val_at(&MODULE()->constants, k))

#if cGAB_DEBUG_VM
#define PUSH(value)                                                            \
  ({                                                                           \
    if (TOP() > (SLOTS() + CLOSURE()->p->nslots + 1)) {                        \
      fprintf(stderr,                                                          \
              "Stack exceeded frame "                                          \
              "(%d). %lu passed\n",                                            \
              CLOSURE()->p->nslots, TOP() - SLOTS() - CLOSURE()->p->nslots);   \
      gab_pry(VM(), 0);                                                        \
      exit(1);                                                                 \
    }                                                                          \
    *TOP()++ = value;                                                          \
  })

#else
#define PUSH(value) (*TOP()++ = value)
#endif
#define POP() (*(--TOP()))
#define DROP() (TOP()--)
#define POP_N(n) (TOP() -= (n))
#define DROP_N(n) (TOP() -= (n))
#define PEEK() (*(TOP() - 1))
#define PEEK2() (*(TOP() - 2))
#define PEEK_N(n) (*(TOP() - (n)))

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

#define STORE_FRAME() ({ FRAME()->ip = IP(); })
#define LOAD_FRAME() ({ IP() = FRAME()->ip; })

#define SEND_CACHE_DIST 22
#define PROP_CACHE_DIST 19

#define ERROR(status, help, ...)                                               \
  (a_gab_value_one(vm_error(GAB(), flags, status, help, __VA_ARGS__)))

  /*
   ----------- BEGIN RUN BODY -----------
  */
  VM() = NEW(struct gab_vm);
  gab_vmcreate(VM(), flags, argc, argv);

  register uint8_t instr = OP_NOP;
  register uint8_t *ip = NULL;

  *VM()->sp++ = main;
  for (uint8_t i = 0; i < argc; i++)
    *VM()->sp++ = argv[i];

  switch (gab_valknd(main)) {
  case kGAB_BLOCK: {
    struct gab_obj_block *b = GAB_VAL_TO_BLOCK(main);
    if (!call_block(VM(), b, argc, VAR_EXP))
      return ERROR(GAB_OVERFLOW, "", "");
    break;
  }
  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *s = GAB_VAL_TO_SUSPENSE(main);
    if (!call_suspense(VM(), s, argc, VAR_EXP))
      return ERROR(GAB_OVERFLOW, "", "");
    break;
  }
  default:
    return a_gab_value_one(gab_nil);
  }

  LOAD_FRAME();

  NEXT();

  LOOP() {
    CASE_CODE(SEND_ANA) : {
      gab_value m = READ_CONSTANT;

      uint8_t have = parse_have(VM(), READ_BYTE);
      SKIP_BYTE;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_valtyp(EG(), receiver);

      uint64_t offset = gab_msgfind(m, type);

      if (offset == UINT64_MAX) {
        if (gab_valknd(receiver) == kGAB_RECORD) {
          offset = gab_msgfind(m, gab_typ(EG(), kGAB_RECORD));

          if (offset != UINT64_MAX)
            goto fin;
        }

        offset = gab_msgfind(m, gab_undefined);

        if (offset == UINT64_MAX) {
          STORE_FRAME();
          return ERROR(GAB_IMPLEMENTATION_MISSING,
                       "%V does not specialize on %V: %V", m, type, receiver);
        }
      }

    fin : {
      gab_value spec = gab_umsgat(m, offset);

      WRITE_INLINEBYTE(GAB_VAL_TO_MESSAGE(m)->version);
      WRITE_INLINEQWORD(type);
      WRITE_INLINEQWORD(offset);

      switch (gab_valknd(spec)) {
      case kGAB_PRIMITIVE:
        WRITE_BYTE(SEND_CACHE_DIST, gab_valtop(spec));
        break;
      case kGAB_BLOCK:
        WRITE_BYTE(SEND_CACHE_DIST, instr + 1);
        break;
      case kGAB_BUILTIN:
        WRITE_BYTE(SEND_CACHE_DIST, instr + 2);
        break;
      default:
        STORE_FRAME();
        return ERROR(GAB_NOT_CALLABLE, "Found %V %V", type, spec);
      }

      IP() -= SEND_CACHE_DIST;

      NEXT();
    }
    }

    CASE_CODE(SEND_MONO_CLOSURE) : {
      gab_value m = READ_CONSTANT;

      uint8_t have = parse_have(VM(), READ_BYTE);
      uint8_t want = READ_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      uint64_t offset = *READ_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_valtyp(EG(), receiver);

      if ((cached_type != type) | (version != GAB_VAL_TO_MESSAGE(m)->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      gab_value spec = gab_umsgat(m, offset);

      struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

      STORE_FRAME();

      if (!call_block(VM(), blk, have, want))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_MONO_BUILTIN) : {
      gab_value m = READ_CONSTANT;

      uint8_t have = parse_have(VM(), READ_BYTE);
      uint8_t want = READ_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      uint64_t offset = *READ_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_valtyp(EG(), receiver);

      if ((cached_type != type) | (version != GAB_VAL_TO_MESSAGE(m)->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      gab_value spec = gab_umsgat(m, offset);

      STORE_FRAME();

      call_builtin(GAB(), GAB_VAL_TO_BUILTIN(spec), have, want, true);

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CALL_BUILTIN) : {
      gab_value m = READ_CONSTANT;
      uint8_t have = parse_have(VM(), READ_BYTE);
      uint8_t want = READ_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_valtyp(EG(), receiver);

      if ((cached_type != type) | (version != GAB_VAL_TO_MESSAGE(m)->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      STORE_FRAME();

      call_builtin(GAB(), GAB_VAL_TO_BUILTIN(receiver), have, want, false);

      LOAD_FRAME();
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CALL_BLOCK) : {
      gab_value msg = READ_CONSTANT;
      uint8_t have = parse_have(VM(), READ_BYTE);
      uint8_t want = READ_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_valtyp(EG(), receiver);

      if ((cached_type != type) |
          (version != GAB_VAL_TO_MESSAGE(msg)->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      STORE_FRAME();

      struct gab_obj_block *blk = GAB_VAL_TO_BLOCK(receiver);

      if (!call_block(VM(), blk, have, want))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CALL_SUSPENSE) : {
      gab_value m = READ_CONSTANT;
      uint8_t have = parse_have(VM(), READ_BYTE);
      uint8_t want = READ_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_valtyp(EG(), receiver);

      if ((cached_type != type) | (version != GAB_VAL_TO_MESSAGE(m)->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      STORE_FRAME();

      if (!call_suspense(VM(), GAB_VAL_TO_SUSPENSE(receiver), have, want))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_ADD) : {
      BINARY_PRIMITIVE(gab_number, double, +);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_SUB) : {
      BINARY_PRIMITIVE(gab_number, double, -);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_MUL) : {
      BINARY_PRIMITIVE(gab_number, double, *);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_DIV) : {
      BINARY_PRIMITIVE(gab_number, double, /);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_MOD) : {
      BINARY_PRIMITIVE(gab_number, uint64_t, %);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_BOR) : {
      BINARY_PRIMITIVE(gab_number, uint64_t, |);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_BND) : {
      BINARY_PRIMITIVE(gab_number, uint64_t, &);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LSH) : {
      BINARY_PRIMITIVE(gab_number, uint64_t, <<);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_RSH) : {
      BINARY_PRIMITIVE(gab_number, uint64_t, >>);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LT) : {
      BINARY_PRIMITIVE(gab_bool, double, <);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LTE) : {
      BINARY_PRIMITIVE(gab_bool, double, <=);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_GT) : {
      BINARY_PRIMITIVE(gab_bool, double, >);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_GTE) : {
      BINARY_PRIMITIVE(gab_bool, double, >=);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CONCAT) : {
      SKIP_SHORT;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_QWORD;
      SKIP_QWORD;

      if (gab_valknd(PEEK2()) != kGAB_STRING) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      if (gab_valknd(PEEK()) != kGAB_STRING) {
        STORE_FRAME();
        return ERROR(GAB_NOT_STRING, "Found %V %V", gab_valtyp(EG(), PEEK()),
                     PEEK());
      }

      gab_value b = POP();
      gab_value a = POP();
      gab_value ab = gab_strcat(GAB(), a, b);

      PUSH(ab);

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_EQ) : {
      gab_value msg = READ_CONSTANT;
      SKIP_BYTE;
      SKIP_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK2();

      gab_value type = gab_valtyp(EG(), receiver);

      if ((cached_type != type) |
          (version != GAB_VAL_TO_MESSAGE(msg)->version)) {
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

    CASE_CODE(SEND_PRIMITIVE_STORE) : {
      gab_value msg = READ_CONSTANT;
      SKIP_BYTE;
      SKIP_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value value = PEEK();
      gab_value key = PEEK2();
      gab_value index = PEEK_N(3);

      gab_value type = gab_valtyp(EG(), index);

      if ((cached_type != type) |
          (version != GAB_VAL_TO_MESSAGE(msg)->version)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      uint64_t prop_offset = gab_shpfind(type, key);

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "On %V", index);
      }

      gab_urecput(index, prop_offset, value);

      DROP_N(3);

      PUSH(value);

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LOAD) : {
      gab_value msg = READ_CONSTANT;
      SKIP_BYTE;
      SKIP_BYTE;
      uint8_t version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value key = PEEK();
      gab_value index = PEEK2();

      gab_value type = gab_valtyp(EG(), index);

      /*
       * This opcode is optimized for
       * loading properties from records.
       *
       * We enter this opcode because we
       * have seen a record as the receiver
       * before.
       *
       * In this case, the type of the
       * receiver and the shape of the
       * record are the SAME, and it is the
       * cached_type.
       *
       * Therefore, if we have the cached
       * type that we expect, then our
       * record also has the shape that we
       * expect.
       *
       * However, we don't cache the offset
       * because the key can change at every
       * invocation and we don't have space
       * to cache that.
       */

      if ((cached_type != type) |
          (version != GAB_VAL_TO_MESSAGE(msg)->version)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      uint16_t prop_offset = gab_shpfind(type, key);

      struct gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

      DROP_N(2);

      gab_value val =
          prop_offset == UINT16_MAX ? gab_nil : obj->data[prop_offset];

      PUSH(val);

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(ITER) : {
      uint8_t want = READ_BYTE;
      uint8_t start = READ_BYTE;
      uint16_t dist = READ_SHORT;
      uint64_t have = VAR();

      gab_value sus = POP();
      have--;

      IP() += dist * (gab_valknd(sus) != kGAB_SUSPENSE);

      trim_values(TOP() - have, SLOTS() + start, have, want);

      DROP_N(have);

      LOCAL(start + want) = sus;

      NEXT();
    }

    CASE_CODE(NEXT) : {
      uint8_t iterator = READ_BYTE;

      PUSH(LOCAL(iterator));

      STORE_FRAME();

      if (!call_suspense(VM(), GAB_VAL_TO_SUSPENSE(PEEK()), 0, VAR_EXP))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    {

      uint8_t have;

      {
        CASE_CODE(YIELD) : {
          gab_value proto = READ_CONSTANT;
          have = parse_have(VM(), READ_BYTE);

          uint64_t frame_len = TOP() - SLOTS() - have;

          assert(frame_len < UINT16_MAX);

          gab_value sus = gab_suspense(GAB(), frame_len, __gab_obj(CLOSURE()),
                                       proto, SLOTS());

          PUSH(sus);

          have++;

          goto complete_return;
        }
      }

      CASE_CODE(RETURN) : {
        have = parse_have(VM(), READ_BYTE);

        goto complete_return;
      }

    complete_return : {
      gab_value *from = TOP() - have;
      gab_value *to = FRAME()->slots;

      TOP() = trim_return(from, to, have, FRAME()->want);

      if (--FRAME() == VM()->fb) {
        gab_ngciref(GAB(), 1, have, to);

        a_gab_value *results = a_gab_value_create(to, have);

        VM()->sp = VM()->sb;

        gab_vmdestroy(EG(), VM());

        DESTROY(VM());
        GAB().vm = NULL;

        return results;
      }

      LOAD_FRAME();

      NEXT();
    }
    }

    /*
     * We haven't seen any shapes yet.
     *
     * Prime the cache and dispatch to mono
     */
    CASE_CODE(LOAD_PROPERTY_ANA) : {
      gab_value key = READ_CONSTANT;
      gab_value index = PEEK();

      if (gab_valknd(index) != kGAB_RECORD) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V %V", gab_valtyp(EG(), index),
                     index);
      }

      struct gab_obj_record *rec = GAB_VAL_TO_RECORD(index);
      uint64_t prop_offset = gab_shpfind(rec->shape, key);

      // Transition state and store in the
      // ache
      WRITE_INLINEQWORD(prop_offset);
      WRITE_INLINEQWORD(rec->shape);
      WRITE_BYTE(PROP_CACHE_DIST, OP_LOAD_PROPERTY_MONO);

      IP() -= PROP_CACHE_DIST;

      NEXT();
    }

    CASE_CODE(LOAD_PROPERTY_MONO) : {
      SKIP_SHORT;
      uint64_t prop_offset = *READ_QWORD;
      gab_value cached_shape = *READ_QWORD;

      gab_value index = PEEK();

      if (gab_valknd(index) != kGAB_RECORD) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V %V", gab_valtyp(EG(), index),
                     index);
      }

      struct gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      if (cached_shape != rec->shape) {
        WRITE_BYTE(PROP_CACHE_DIST, OP_LOAD_PROPERTY_POLY);
        IP() -= PROP_CACHE_DIST;
        NEXT();
      }

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "On %V", index);
      }

      PEEK() = rec->data[prop_offset];

      NEXT();
    }

    CASE_CODE(LOAD_PROPERTY_POLY) : {
      gab_value key = READ_CONSTANT;
      SKIP_QWORD;
      SKIP_QWORD;

      gab_value index = PEEK();

      if (gab_valknd(index) != kGAB_RECORD) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V %V", gab_valtyp(EG(), index),
                     index);
      }

      struct gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      uint64_t prop_offset = gab_shpfind(rec->shape, key);

      DROP();

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "On %V", index);
      }

      PEEK() = rec->data[prop_offset];

      NEXT();
    }

    CASE_CODE(STORE_PROPERTY_ANA) : {
      gab_value key = READ_CONSTANT;

      gab_value index = PEEK2();

      if (gab_valknd(index) != kGAB_RECORD) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V %V", gab_valtyp(EG(), index),
                     index);
      }

      struct gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      uint64_t prop_offset = gab_shpfind(rec->shape, key);

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "On %V", index);
      }

      // Write to the cache and transition
      // to monomorphic
      WRITE_INLINEQWORD(prop_offset);
      WRITE_INLINEQWORD((uint64_t)rec->shape);
      WRITE_BYTE(PROP_CACHE_DIST, OP_STORE_PROPERTY_MONO);

      IP() -= PROP_CACHE_DIST;

      NEXT();
    }

    CASE_CODE(STORE_PROPERTY_MONO) : {
      gab_value key = READ_CONSTANT;
      // Use the cache
      uint64_t prop_offset = *READ_QWORD;
      gab_value cached_shape = *READ_QWORD;

      gab_value value = PEEK();
      gab_value index = PEEK2();

      if (gab_valknd(index) != kGAB_RECORD) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V %V", gab_valtyp(EG(), index),
                     index);
      }

      struct gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      if (rec->shape != cached_shape) {
        prop_offset = gab_shpfind(rec->shape, key);

        if (prop_offset == UINT64_MAX) {
          STORE_FRAME();
          return ERROR(GAB_MISSING_PROPERTY, "On %V", index);
        }

        // Transition to polymorphic
        WRITE_BYTE(PROP_CACHE_DIST, OP_STORE_PROPERTY_POLY);
      }

      gab_urecput(index, prop_offset, value);

      DROP_N(2);

      PUSH(value);

      NEXT();
    }

    CASE_CODE(STORE_PROPERTY_POLY) : {
      gab_value key = READ_CONSTANT;
      // Skip the cache
      SKIP_SHORT;
      SKIP_QWORD;

      gab_value value = PEEK();
      gab_value index = PEEK2();

      if (gab_valknd(index) != kGAB_RECORD) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V %V", gab_valtyp(EG(), index),
                     index);
      }

      struct gab_obj_record *rec = GAB_VAL_TO_RECORD(index);
      uint64_t prop_offset = gab_shpfind(rec->shape, key);

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "On %V", index);
      }

      gab_urecput(index, prop_offset, value);

      DROP_N(2);

      PUSH(value);

      NEXT();
    }

    CASE_CODE(NOP) : { NEXT(); }

    CASE_CODE(CONSTANT) : {
      PUSH(READ_CONSTANT);
      NEXT();
    }

    CASE_CODE(SHIFT) : {
      uint8_t n = READ_BYTE;

      gab_value tmp = PEEK();

      memcpy(TOP() - (n - 1), TOP() - n, (n - 1) * sizeof(gab_value));

      PEEK_N(n) = tmp;

      NEXT();
    }

    CASE_CODE(POP) : {
      DROP();
      NEXT();
    }

    CASE_CODE(POP_N) : {
      DROP_N(READ_BYTE);
      NEXT();
    }

    CASE_CODE(DUP) : {
      gab_value a = PEEK();
      PUSH(a);
      NEXT();
    }

    CASE_CODE(SWAP) : {
      gab_value tmp = PEEK();
      PEEK() = PEEK2();
      PEEK2() = tmp;
      NEXT();
    }

    CASE_CODE(INTERPOLATE) : {
      uint8_t n = READ_BYTE;
      gab_value str = gab_valintos(GAB(), PEEK_N(n));

      for (uint8_t i = n - 1; i > 0; i--) {
        gab_value curr = gab_valintos(GAB(), PEEK_N(i));
        str = gab_strcat(GAB(), str, curr);
      }

      POP_N(n);

      PUSH(str);

      NEXT();
    }

    CASE_CODE(LOGICAL_AND) : {
      uint16_t offset = READ_SHORT;
      gab_value cond = PEEK();

      if (gab_valintob(cond))
        DROP();
      else
        ip += offset;

      NEXT();
    }

    CASE_CODE(LOGICAL_OR) : {
      uint16_t offset = READ_SHORT;
      gab_value cond = PEEK();

      if (gab_valintob(cond))
        ip += offset;
      else
        DROP();

      NEXT();
    }

    CASE_CODE(PUSH_NIL) : {
      PUSH(gab_nil);
      NEXT();
    }

    CASE_CODE(PUSH_UNDEFINED) : {
      PUSH(gab_undefined);
      NEXT();
    }

    CASE_CODE(PUSH_TRUE) : {
      PUSH(gab_bool(true));
      NEXT();
    }

    CASE_CODE(PUSH_FALSE) : {
      PUSH(gab_bool(false));
      NEXT();
    }

    CASE_CODE(NEGATE) : {
      if (!__gab_valisn(PEEK())) {
        STORE_FRAME();
        return ERROR(GAB_NOT_NUMERIC, "Found %V %V", gab_valtyp(EG(), PEEK()),
                     PEEK());
      }

      gab_value new_value = gab_number(-gab_valton(POP()));

      PUSH(new_value);

      NEXT();
    }

    CASE_CODE(NOT) : {
      gab_value new_value = gab_bool(gab_valintob(POP()));
      PUSH(new_value);
      NEXT();
    }

    CASE_CODE(TYPE) : {
      PEEK() = gab_valtyp(EG(), PEEK());
      NEXT();
    }

    CASE_CODE(MATCH) : {
      gab_value test = POP();
      gab_value pattern = PEEK();
      if (test == pattern) {
        DROP();
        PUSH(gab_bool(true));
      } else {
        PUSH(gab_bool(false));
      }
      NEXT();
    }

    // clang-format off

    CASE_CODE(LOAD_LOCAL_0):
    CASE_CODE(LOAD_LOCAL_1):
    CASE_CODE(LOAD_LOCAL_2):
    CASE_CODE(LOAD_LOCAL_3):
    CASE_CODE(LOAD_LOCAL_4):
    CASE_CODE(LOAD_LOCAL_5):
    CASE_CODE(LOAD_LOCAL_6):
    CASE_CODE(LOAD_LOCAL_7):
    CASE_CODE(LOAD_LOCAL_8): {
      PUSH(LOCAL(INSTR()- OP_LOAD_LOCAL_0));
      NEXT();
    }

    CASE_CODE(STORE_LOCAL_0):
    CASE_CODE(STORE_LOCAL_1):
    CASE_CODE(STORE_LOCAL_2):
    CASE_CODE(STORE_LOCAL_3):
    CASE_CODE(STORE_LOCAL_4):
    CASE_CODE(STORE_LOCAL_5):
    CASE_CODE(STORE_LOCAL_6):
    CASE_CODE(STORE_LOCAL_7):
    CASE_CODE(STORE_LOCAL_8): {
      LOCAL(INSTR()- OP_STORE_LOCAL_0) = PEEK();
      NEXT();
    }

    CASE_CODE(POP_STORE_LOCAL_0):
    CASE_CODE(POP_STORE_LOCAL_1):
    CASE_CODE(POP_STORE_LOCAL_2):
    CASE_CODE(POP_STORE_LOCAL_3):
    CASE_CODE(POP_STORE_LOCAL_4):
    CASE_CODE(POP_STORE_LOCAL_5):
    CASE_CODE(POP_STORE_LOCAL_6):
    CASE_CODE(POP_STORE_LOCAL_7):
    CASE_CODE(POP_STORE_LOCAL_8): {
      LOCAL(INSTR()- OP_POP_STORE_LOCAL_0) = POP();
      NEXT();
    }

    CASE_CODE(LOAD_UPVALUE_0) :
    CASE_CODE(LOAD_UPVALUE_1) :
    CASE_CODE(LOAD_UPVALUE_2) :
    CASE_CODE(LOAD_UPVALUE_3) :
    CASE_CODE(LOAD_UPVALUE_4) :
    CASE_CODE(LOAD_UPVALUE_5) :
    CASE_CODE(LOAD_UPVALUE_6) :
    CASE_CODE(LOAD_UPVALUE_7) :
    CASE_CODE(LOAD_UPVALUE_8) : {
      PUSH(UPVALUE(INSTR() - OP_LOAD_UPVALUE_0));
      NEXT();
    }

    // clang-format on

    CASE_CODE(LOAD_LOCAL) : {
      PUSH(LOCAL(READ_BYTE));
      NEXT();
    }

    CASE_CODE(STORE_LOCAL) : {
      LOCAL(READ_BYTE) = PEEK();
      NEXT();
    }

    CASE_CODE(POP_STORE_LOCAL) : {
      LOCAL(READ_BYTE) = POP();
      NEXT();
    }

    CASE_CODE(LOAD_UPVALUE) : {
      PUSH(UPVALUE(READ_BYTE));
      NEXT();
    }

    CASE_CODE(POP_JUMP_IF_TRUE) : {
      uint16_t dist = READ_SHORT;
      ip += dist * gab_valintob(POP());
      NEXT();
    }

    CASE_CODE(POP_JUMP_IF_FALSE) : {
      uint16_t dist = READ_SHORT;
      ip += dist * !gab_valintob(POP());
      NEXT();
    }

    CASE_CODE(JUMP_IF_TRUE) : {
      uint16_t dist = READ_SHORT;
      ip += dist * gab_valintob(PEEK());
      NEXT();
    }

    CASE_CODE(JUMP_IF_FALSE) : {
      uint16_t dist = READ_SHORT;
      ip += dist * !gab_valintob(PEEK());
      NEXT();
    }

    CASE_CODE(JUMP) : {
      uint16_t dist = READ_SHORT;
      ip += dist;
      NEXT();
    }

    CASE_CODE(LOOP) : {
      uint16_t dist = READ_SHORT;
      ip -= dist;
      NEXT();
    }

    CASE_CODE(BLOCK) : {
      gab_value p = READ_CONSTANT;

      gab_value blk = gab_block(GAB(), p);

      struct gab_obj_block *b = GAB_VAL_TO_BLOCK(blk);
      struct gab_obj_block_proto *proto = GAB_VAL_TO_BLOCK_PROTO(p);

      for (int i = 0; i < proto->nupvalues; i++) {
        uint8_t flags = proto->upv_desc[i * 2];
        uint8_t index = proto->upv_desc[i * 2 + 1];

        if (flags & fVAR_LOCAL) {
          b->upvalues[i] = LOCAL(index);
        } else {
          b->upvalues[i] = CLOSURE()->upvalues[index];
        }
      }

      PUSH(blk);

      NEXT();
    }

    CASE_CODE(MESSAGE) : {
      gab_value p = READ_CONSTANT;
      gab_value m = READ_CONSTANT;
      gab_value r = PEEK();

      uint64_t offset = gab_msgfind(m, r);

      if (offset != UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_IMPLEMENTATION_EXISTS,
                     " Tried to specialize %V for %V", m, r);
      }

      gab_value blk = gab_block(GAB(), p);

      struct gab_obj_block *b = GAB_VAL_TO_BLOCK(blk);
      struct gab_obj_block_proto *proto = GAB_VAL_TO_BLOCK_PROTO(p);

      for (int i = 0; i < b->nupvalues; i++) {
        uint8_t flags = proto->upv_desc[i * 2];
        uint8_t index = proto->upv_desc[i * 2 + 1];

        if (flags & fVAR_LOCAL) {
          assert(index < BLOCK_PROTO()->nlocals);
          b->upvalues[i] = LOCAL(index);
        } else {
          assert(index < CLOSURE()->nupvalues);
          b->upvalues[i] = CLOSURE()->upvalues[index];
        }
      }

      struct gab_obj_message *msg = GAB_VAL_TO_MESSAGE(m);

      gab_gcdref(GAB(), msg->specs);

      msg->specs = gab_recordwith(GAB(), msg->specs, r, blk);
      msg->version++;

      gab_gciref(GAB(), msg->specs);

      PEEK() = m;

      NEXT();
    }

    CASE_CODE(VAR) : {
      VAR() = READ_BYTE;
      NEXT();
    }

    CASE_CODE(PACK) : {
      uint8_t below = READ_BYTE;
      uint8_t above = READ_BYTE;

      uint64_t want = below + above;
      uint64_t have = VAR();

      while (have < want)
        PUSH(gab_nil), have++;

      uint64_t len = have - want;

      gab_value *ap = TOP() - above;

      gab_value shape = gab_nshape(GAB(), len);

      gab_value rec = gab_recordof(GAB(), shape, 1, ap - len);

      DROP_N(len - 1);

      memcpy(ap - len + 1, ap, above * sizeof(gab_value));

      PEEK_N(above + 1) = rec;

      VAR() = want + 1;

      NEXT();
    }

    CASE_CODE(RECORD) : {
      uint8_t len = READ_BYTE;

      gab_value shape = gab_shape(GAB(), 2, len, TOP() - len * 2);

      gab_value rec = gab_recordof(GAB(), shape, 2, TOP() + 1 - (len * 2));

      DROP_N(len * 2);

      PUSH(rec);

      NEXT();
    }

    CASE_CODE(TUPLE) : {
      uint8_t len = parse_have(VM(), READ_BYTE);

      gab_value shape = gab_nshape(GAB(), len);

      gab_value rec = gab_recordof(GAB(), shape, 1, TOP() - len);

      DROP_N(len);

      PUSH(rec);

      NEXT();
    }
  }

  // This should be unreachable
  return a_gab_value_one(gab_nil);
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_PRIMITIVE
