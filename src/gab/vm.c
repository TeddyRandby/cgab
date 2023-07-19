#include "include/vm.h"
#include "include/char.h"
#include "include/colors.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/module.h"
#include "include/object.h"
#include "include/value.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
};

void gab_vm_container_cb(void *data) { DESTROY(data); }

gab_value vm_error(gab_engine *gab, gab_vm *vm, u8 flags, gab_status e,
                   const char *help_fmt, ...) {
  u64 nframes = vm->fp - vm->fb;
  u64 n = 1;

  if (flags & fGAB_DUMP_ERROR) {
    for (;;) {
      gab_vm_frame *frame = vm->fb + n;

      s_i8 func_name =
          gab_obj_string_ref(GAB_VAL_TO_STRING(v_gab_constant_val_at(
              &frame->b->p->mod->constants, frame->b->p->mod->name)));

      gab_module *mod = frame->b->p->mod;

      if (!mod->source)
        break;

      u64 offset = frame->ip - mod->bytecode.data;

      u64 curr_row = v_u64_val_at(&mod->lines, offset);

      s_i8 curr_src = v_s_i8_val_at(&mod->source->source_lines, curr_row - 1);

      const i8 *curr_src_start = curr_src.data;
      i32 curr_src_len = curr_src.len;

      while (is_whitespace(*curr_src_start)) {
        curr_src_start++;
        curr_src_len--;
        if (curr_src_len == 0)
          break;
      }

      const char *tok = gab_token_names[v_u8_val_at(&mod->tokens, offset)];

      s_i8 curr_token = v_s_i8_val_at(&mod->sources, offset);
      a_i8 *curr_under = a_i8_empty(curr_src_len);

      const i8 *tok_start, *tok_end;
      tok_start = curr_token.data;
      tok_end = curr_token.data + curr_token.len;

      for (u8 i = 0; i < curr_under->len; i++) {
        if (curr_src_start + i >= tok_start && curr_src_start + i < tok_end)
          curr_under->data[i] = '^';
        else
          curr_under->data[i] = ' ';
      }

      if (n >= nframes) {
        fprintf(stderr,
                "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET
                "] Error near " ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET
                ":\n\t\u256d " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%.*s"
                "\n\t\u2502      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET
                "\n\t\u2570\u2500> ",
                (i32)func_name.len, func_name.data, tok, curr_row, curr_src_len,
                curr_src_start, (i32)curr_under->len, curr_under->data);

        a_i8_destroy(curr_under);

        fprintf(stderr,
                ANSI_COLOR_YELLOW "%s.\n\n\t" ANSI_COLOR_RESET ANSI_COLOR_GREEN,
                gab_status_names[e]);

        va_list args;
        va_start(args, help_fmt);

        vfprintf(stderr, help_fmt, args);

        va_end(args);

        fprintf(stderr, "\n" ANSI_COLOR_RESET);

        break;
      }

      fprintf(stderr,
              "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET "] Called at:"
              "\n\t  " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%.*s"
              "\n\t       " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET "\n",
              (i32)func_name.len, func_name.data, curr_row, curr_src_len,
              curr_src_start, (i32)curr_under->len, curr_under->data);

      n++;
    }
  }

  if (flags & fGAB_EXIT_ON_PANIC) {
    exit(0);
  }

  return GAB_CONTAINER(GAB_STRING("gab_vm"), gab_vm_container_cb, NULL, vm);
}

gab_value gab_vm_panic(gab_engine *gab, gab_vm *vm, const char *msg) {
  return vm_error(gab, vm, fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC, GAB_PANIC,
                  msg);
}

void gab_vm_create(gab_vm *self, u8 flags, u8 argc, gab_value argv[argc]) {
  gab_gc_create(&self->gc);

  self->fp = self->fb;
  self->sp = self->sb;
  self->fp->slots = self->sp;
  self->flags = flags;
}

void gab_vm_destroy(gab_vm *self) {
  gab_gc_run(&self->gc, self);
  gab_gc_destroy(&self->gc);
}

void gab_pry(gab_vm *vm, u64 value) {
  u64 frame_count = vm->fp - vm->fb;

  if (value >= frame_count)
    return;

  gab_vm_frame *f = vm->fp - value;

  s_i8 func_name = gab_obj_string_ref(GAB_VAL_TO_STRING(
      v_gab_constant_val_at(&f->b->p->mod->constants, f->b->p->mod->name)));

  printf(ANSI_COLOR_GREEN " %03lu" ANSI_COLOR_RESET " closure:" ANSI_COLOR_CYAN
                          "%-20.*s" ANSI_COLOR_RESET " %d upvalues\n",
         frame_count - value, (i32)func_name.len, func_name.data,
         f->b->p->nupvalues);

  for (i32 i = f->b->p->nslots - 1; i >= 0; i--) {
    printf("%2s" ANSI_COLOR_YELLOW "%4i " ANSI_COLOR_RESET "%V\n",
           vm->sp == f->slots + i ? "->" : "", i, f->slots[i]);
  }
}

static inline i32 parse_have(gab_vm *vm, u8 have) {
  if (have & FLAG_VAR_EXP)
    return *vm->sp + (have >> 1);
  else
    return have >> 1;
}

static inline u64 trim_values(gab_value *from, gab_value *to, u64 have,
                              u8 want) {
  u64 nulls = 0;

  if ((have != want) && (want != VAR_EXP)) {
    if (have > want)
      have = want;
    else
      nulls = want - have;
  }

  const u64 got = have + nulls;

  while (have--)
    *to++ = *from++;

  while (nulls--)
    *to++ = GAB_VAL_NIL();

  return got;
}

static inline gab_value *trim_return(gab_value *from, gab_value *to, u64 have,
                                     u8 want) {
  u64 got = trim_values(from, to, have, want);

  gab_value *sp = to + got;
  *sp = got;

  return sp;
}

static inline boolean has_callspace(gab_vm *vm, u64 space_needed) {
  if (vm->fp - vm->fb + 1 >= cGAB_FRAMES_MAX) {
    return false;
  }

  if (vm->sp - vm->sb + space_needed >= cGAB_STACK_MAX) {
    return false;
  }

  return true;
}

static inline boolean call_suspense(gab_vm *vm, gab_obj_suspense *sus, u8 have,
                                    u8 want) {
  i16 space_needed = sus->len;

  if (space_needed > 0 && !has_callspace(vm, space_needed))
    return false;

  vm->fp++;
  vm->fp->b = sus->b;
  vm->fp->ip = sus->b->p->mod->bytecode.data + sus->p->offset;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - have - 1;

  gab_value *from = vm->sp - have;
  gab_value *to = vm->fp->slots + sus->len;

  vm->sp = trim_return(from, to, have, sus->p->want);
  memcpy(vm->fp->slots, sus->frame, sus->len * sizeof(gab_value));

  return true;
}

i32 gab_vm_push(gab_vm *vm, u64 argc, gab_value argv[argc]) {
  if (!has_callspace(vm, argc)) {
    return -1;
  }

  for (u8 n = 0; n < argc; n++) {
    *vm->sp++ = argv[n];
  }

  return argc;
}

static inline boolean call_block(gab_vm *vm, gab_obj_block *b, u64 have,
                                 u8 want) {
  u64 len = b->p->narguments == VAR_EXP ? have : b->p->narguments;

  if (!has_callspace(vm, b->p->nslots - len - 1))
    return false;

  vm->fp++;
  vm->fp->b = b;
  vm->fp->ip = b->p->mod->bytecode.data;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - have - 1;

  vm->sp =
      trim_return(vm->fp->slots + 1, vm->fp->slots + 1, have, len);

  return true;
}

static inline void call_builtin(gab_engine *gab, gab_vm *vm, gab_obj_builtin *b,
                                u8 arity, u8 want, boolean is_message) {
  gab_value *to = vm->sp - arity - 1; // Is this -1 correct?

  gab_value *before = vm->sp;

  // Only pass in the extra "self" argument if this is a message.
  (*b->function)(gab, vm, arity + is_message, vm->sp - arity - is_message);

  u8 have = vm->sp - before;

  // There is always an extra to trim bc of the receiver or callee.
  vm->sp = trim_return(vm->sp - have, to, have, want);
}

a_gab_value *gab_vm_run(gab_engine *gab, gab_value main, u8 flags, u8 argc,
                        gab_value argv[argc]) {
#if cGAB_LOG_VM
#define LOG() printf("OP_%s\n", gab_opcode_names[*(ip)])
#else
#define LOG()
#endif

  /*
    If we're using computed gotos, create the jump table and the dispatch
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
  if (!GAB_VAL_IS_NUMBER(PEEK2())) {                                           \
    WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);                                  \
    IP() -= SEND_CACHE_DIST;                                                   \
    NEXT();                                                                    \
  }                                                                            \
  if (!GAB_VAL_IS_NUMBER(PEEK())) {                                            \
    STORE_FRAME();                                                             \
    return ERROR(GAB_NOT_NUMERIC, "Found '%V'", PEEK());                       \
  }                                                                            \
  operation_type b = GAB_VAL_TO_NUMBER(POP());                                 \
  operation_type a = GAB_VAL_TO_NUMBER(POP());                                 \
  PUSH(value_type(a operation b));                                             \
  VAR() = 1;

/*
  Lots of helper macros.
*/
#define ENGINE() (gab)
#define GC() (&VM()->gc)
#define VM() (vm)
#define INSTR() (instr)
#define FRAME() (VM()->fp)
#define CLOSURE() (FRAME()->b)
#define MODULE() (CLOSURE()->p->mod)
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
      fprintf(stderr, "Stack exceeded frame (%d). %lu passed\n",               \
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
#define WRITE_INLINEQWORD(n) (IP() += 8, *((u64 *)(IP() - 8)) = n)

#define SKIP_BYTE (IP()++)
#define SKIP_SHORT (IP() += 2)
#define SKIP_QWORD (IP() += 8)

#define READ_BYTE (*IP()++)
#define READ_SHORT (IP() += 2, (((u16)IP()[-2] << 8) | IP()[-1]))
#define READ_QWORD (IP() += 8, (u64 *)(IP() - 8))

#define READ_CONSTANT (MOD_CONSTANT(READ_SHORT))
#define READ_STRING (GAB_VAL_TO_STRING(READ_CONSTANT))
#define READ_MESSAGE (GAB_VAL_TO_MESSAGE(READ_CONSTANT))
#define READ_BLOCK_PROTOTYPE (GAB_VAL_TO_BLOCK_PROTO(READ_CONSTANT))
#define READ_SUSPENSE_PROTOTYPE (GAB_VAL_TO_SUSPENSE_PROTO(READ_CONSTANT))

#define STORE_FRAME() ({ FRAME()->ip = IP(); })
#define LOAD_FRAME() ({ IP() = FRAME()->ip; })

#define SEND_CACHE_DIST 22
#define PROP_CACHE_DIST 19

#define ERROR(status, help, ...)                                               \
  (a_gab_value_one(vm_error(ENGINE(), VM(), flags, status, help, __VA_ARGS__)))

  /*
   ----------- BEGIN RUN BODY -----------
  */
  gab_vm *vm = NEW(gab_vm);
  gab_vm_create(vm, flags, argc, argv);

  register u8 instr = OP_NOP;
  register u8 *ip = NULL;

  *vm->sp++ = main;
  for (u8 i = 0; i < argc; i++)
    *vm->sp++ = argv[i];

  if (GAB_VAL_IS_BLOCK(main)) {
    gab_obj_block *b = GAB_VAL_TO_BLOCK(main);
    if (!call_block(VM(), b, argc, 1))
      return ERROR(GAB_OVERFLOW, "", "");
  } else if (GAB_VAL_IS_SUSPENSE(main)) {
    gab_obj_suspense *s = GAB_VAL_TO_SUSPENSE(main);
    if (!call_suspense(VM(), s, argc, 1))
      return ERROR(GAB_OVERFLOW, "", "");
  } else {
    return a_gab_value_one(GAB_VAL_NIL());
  }

  LOAD_FRAME();

  NEXT();

  LOOP() {
    CASE_CODE(SEND_ANA) : {
      gab_obj_message *msg = READ_MESSAGE;
      u8 have = parse_have(VM(), READ_BYTE);
      SKIP_BYTE;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_val_type(ENGINE(), receiver);

      u64 offset = gab_obj_message_find(msg, type);

      if (offset == UINT64_MAX) {
        if (GAB_VAL_IS_RECORD(receiver)) {
          offset = gab_obj_message_find(msg, gab_type(ENGINE(), kGAB_RECORD));

          if (offset != UINT64_MAX)
            goto fin;
        }

        offset = gab_obj_message_find(msg, GAB_VAL_UNDEFINED());

        if (offset == UINT64_MAX) {
          STORE_FRAME();
          return ERROR(GAB_IMPLEMENTATION_MISSING, "Could not send %V to '%V'",
                       GAB_VAL_OBJ(msg), receiver);
        }
      }

    fin : {
      gab_value spec = gab_obj_message_get(msg, offset);

      WRITE_INLINEBYTE(msg->version);
      WRITE_INLINEQWORD(type);
      WRITE_INLINEQWORD(offset);

      if (GAB_VAL_IS_PRIMITIVE(spec)) {
        u8 op = GAB_VAL_TO_PRIMITIVE(spec);
        WRITE_BYTE(SEND_CACHE_DIST, op);
      } else if (GAB_VAL_IS_BLOCK(spec)) {
        WRITE_BYTE(SEND_CACHE_DIST, instr + 1);
      } else if (GAB_VAL_IS_BUILTIN(spec)) {
        WRITE_BYTE(SEND_CACHE_DIST, instr + 2);
      } else {
        STORE_FRAME();
        return ERROR(GAB_NOT_CALLABLE, "Found '%V'", spec);
      }

      IP() -= SEND_CACHE_DIST;

      NEXT();
    }
    }

    CASE_CODE(SEND_MONO_CLOSURE) : {
      gab_obj_message *msg = READ_MESSAGE;
      u8 have = parse_have(VM(), READ_BYTE);
      u8 want = READ_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      u64 offset = *READ_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_val_type(ENGINE(), receiver);

      if ((cached_type != type) | (version != msg->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      gab_value spec = gab_obj_message_get(msg, offset);

      gab_obj_block *blk = GAB_VAL_TO_BLOCK(spec);

      STORE_FRAME();

      if (!call_block(VM(), blk, have, want))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_MONO_BUILTIN) : {
      gab_obj_message *msg = READ_MESSAGE;
      u8 have = parse_have(VM(), READ_BYTE);
      u8 want = READ_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      u64 offset = *READ_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_val_type(ENGINE(), receiver);

      if ((cached_type != type) | (version != msg->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      gab_value spec = gab_obj_message_get(msg, offset);

      STORE_FRAME();

      call_builtin(ENGINE(), VM(), GAB_VAL_TO_BUILTIN(spec), have, want, true);

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CALL_BUILTIN) : {
      gab_obj_message *msg = READ_MESSAGE;
      u8 have = parse_have(VM(), READ_BYTE);
      u8 want = READ_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_val_type(ENGINE(), receiver);

      if ((cached_type != type) | (version != msg->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      STORE_FRAME();

      call_builtin(ENGINE(), VM(), GAB_VAL_TO_BUILTIN(receiver), have, want,
                   false);

      LOAD_FRAME();
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CALL_BLOCK) : {
      gab_obj_message *msg = READ_MESSAGE;
      u8 have = parse_have(VM(), READ_BYTE);
      u8 want = READ_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_val_type(ENGINE(), receiver);

      if ((cached_type != type) | (version != msg->version)) {
        // Revert to anamorphic
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      STORE_FRAME();

      gab_obj_block *blk = GAB_VAL_TO_BLOCK(receiver);

      if (!call_block(VM(), blk, have, want))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CALL_SUSPENSE) : {
      gab_obj_message *msg = READ_MESSAGE;
      u8 have = parse_have(VM(), READ_BYTE);
      u8 want = READ_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK_N(have + 1);

      gab_value type = gab_val_type(ENGINE(), receiver);

      if ((cached_type != type) | (version != msg->version)) {
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
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, f64, +);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_SUB) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, f64, -);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_MUL) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, f64, *);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_DIV) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, f64, /);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_MOD) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, u64, %);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_BOR) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, u64, |);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_BND) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, u64, &);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LSH) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, u64, <<);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_RSH) : {
      BINARY_PRIMITIVE(GAB_VAL_NUMBER, u64, >>);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LT) : {
      BINARY_PRIMITIVE(GAB_VAL_BOOLEAN, f64, <);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LTE) : {
      BINARY_PRIMITIVE(GAB_VAL_BOOLEAN, f64, <=);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_GT) : {
      BINARY_PRIMITIVE(GAB_VAL_BOOLEAN, f64, >);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_GTE) : {
      BINARY_PRIMITIVE(GAB_VAL_BOOLEAN, f64, >=);
      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_CONCAT) : {
      SKIP_SHORT;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_QWORD;
      SKIP_QWORD;

      if (!GAB_VAL_IS_STRING(PEEK2())) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      if (!GAB_VAL_IS_STRING(PEEK())) {
        STORE_FRAME();
        return ERROR(GAB_NOT_STRING, "Found '%V'", PEEK());
      }

      gab_obj_string *b = GAB_VAL_TO_STRING(POP());
      gab_obj_string *a = GAB_VAL_TO_STRING(POP());
      gab_obj_string *ab = gab_obj_string_concat(ENGINE(), a, b);
      PUSH(GAB_VAL_OBJ(ab));

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_EQ) : {
      gab_obj_message *msg = READ_MESSAGE;
      SKIP_BYTE;
      SKIP_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value receiver = PEEK2();

      gab_value type = gab_val_type(ENGINE(), receiver);

      if ((cached_type != type) | (version != msg->version)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      gab_value b = POP();
      gab_value a = POP();

      PUSH(GAB_VAL_BOOLEAN(a == b));

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_STORE) : {
      gab_obj_message *msg = READ_MESSAGE;
      SKIP_BYTE;
      SKIP_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value value = PEEK();
      gab_value key = PEEK2();
      gab_value index = PEEK_N(3);

      gab_value type = gab_val_type(ENGINE(), index);

      if ((cached_type != type) | (version != msg->version)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      u16 prop_offset = gab_obj_shape_find(GAB_VAL_TO_SHAPE(type), key);

      if (prop_offset == UINT16_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "Missing '%V'", key);
      }

      gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

      gab_obj_record_set(VM(), obj, prop_offset, value);

      DROP_N(3);

      PUSH(value);

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LOAD) : {
      gab_obj_message *msg = READ_MESSAGE;
      SKIP_BYTE;
      SKIP_BYTE;
      u8 version = READ_BYTE;
      gab_value cached_type = *READ_QWORD;
      SKIP_QWORD;

      gab_value key = PEEK();
      gab_value index = PEEK2();

      gab_value type = gab_val_type(ENGINE(), index);

      /*
       * This opcode is optimized for loading properties from records.
       *
       * We enter this opcode because we have seen a record as the receiver
       * before.
       *
       * In this case, the type of the receiver and the shape of the record are
       * the SAME, and it is the cached_type.
       *
       * Therefore, if we have the cached type that we expect, then our record
       * also has the shape that we expect.
       *
       * However, we don't cache the offset because the key can change at every
       * invocation and we don't have space to cache that.
       */

      if ((cached_type != type) | (version != msg->version)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      u16 prop_offset = gab_obj_shape_find(GAB_VAL_TO_SHAPE(type), key);

      gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

      DROP_N(2);

      gab_value val = prop_offset == UINT16_MAX
                          ? GAB_VAL_NIL()
                          : gab_obj_record_get(obj, prop_offset);

      PUSH(val);

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(ITER) : {
      u8 want = READ_BYTE;
      u8 start = READ_BYTE;
      u16 dist = READ_SHORT;
      u64 have = VAR();

      gab_value sus = POP();
      have--;

      TOP() = trim_return(TOP() - have, SLOTS() + start, have, want);

      PUSH(sus);

      IP() += dist * !GAB_VAL_IS_SUSPENSE(sus);

      NEXT();
    }

    CASE_CODE(NEXT) : {
      u8 iterator = READ_BYTE;

      STORE_FRAME();

      if (!call_suspense(VM(), GAB_VAL_TO_SUSPENSE(LOCAL(iterator)), 0,
                         VAR_EXP))
        return ERROR(GAB_OVERFLOW, "", "");

      LOAD_FRAME();

      NEXT();
    }

    {

      u8 have;

      {
        CASE_CODE(YIELD) : {
          gab_obj_suspense_proto *proto = READ_SUSPENSE_PROTOTYPE;
          have = parse_have(VM(), READ_BYTE);

          u64 frame_len = TOP() - SLOTS() - have;

          assert(frame_len < UINT16_MAX);

          gab_value sus = GAB_VAL_OBJ(gab_obj_suspense_create(
              ENGINE(), VM(), frame_len, CLOSURE(), proto, SLOTS()));

          PUSH(sus);

          gab_gc_dref(GC(), VM(), sus);

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

      TOP() = trim_return(from, FRAME()->slots, have, FRAME()->want);

      if (--FRAME() == VM()->fb) {
        // Increment and pop the module.
        a_gab_value *results = a_gab_value_create(from, have);

        for (u32 i = 0; i < results->len; i++) {
          gab_gc_iref(GC(), VM(), results->data[i]);
        }

        VM()->sp = VM()->sb;

        gab_vm_destroy(VM());

        DESTROY(VM());

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

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found '%V'", index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);
      u64 prop_offset = gab_obj_shape_find(rec->shape, key);

      // Transition state and store in the ache
      WRITE_INLINEQWORD(prop_offset);
      WRITE_INLINEQWORD(GAB_VAL_OBJ(rec->shape));
      WRITE_BYTE(PROP_CACHE_DIST, OP_LOAD_PROPERTY_MONO);

      IP() -= PROP_CACHE_DIST;

      NEXT();
    }

    CASE_CODE(LOAD_PROPERTY_MONO) : {
      SKIP_SHORT;
      u64 prop_offset = *READ_QWORD;
      gab_obj_shape *cached_shape = GAB_VAL_TO_SHAPE(*READ_QWORD);

      gab_value index = PEEK();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found '%V'", index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      if (cached_shape != rec->shape) {
        WRITE_BYTE(PROP_CACHE_DIST, OP_LOAD_PROPERTY_POLY);
        IP() -= PROP_CACHE_DIST;
        NEXT();
      }

      DROP();

      if (prop_offset == UINT64_MAX)
        PUSH(GAB_VAL_NIL());
      else
        PUSH(gab_obj_record_get(rec, prop_offset));

      NEXT();
    }

    CASE_CODE(LOAD_PROPERTY_POLY) : {
      gab_value key = READ_CONSTANT;
      SKIP_QWORD;
      SKIP_QWORD;

      gab_value index = PEEK();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found '%V'", index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      u64 prop_offset = gab_obj_shape_find(rec->shape, key);

      DROP();

      if (prop_offset == UINT64_MAX)
        PUSH(GAB_VAL_NIL());
      else
        PUSH(gab_obj_record_get(rec, prop_offset));

      NEXT();
    }

    CASE_CODE(STORE_PROPERTY_ANA) : {
      gab_value key = READ_CONSTANT;

      gab_value index = PEEK2();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found '%V'", index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      u64 prop_offset = gab_obj_shape_find(rec->shape, key);

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "Missing '%V'", key);
      }

      // Write to the cache and transition to monomorphic
      WRITE_INLINEQWORD(prop_offset);
      WRITE_INLINEQWORD((u64)rec->shape);
      WRITE_BYTE(PROP_CACHE_DIST, OP_STORE_PROPERTY_MONO);

      IP() -= PROP_CACHE_DIST;

      NEXT();
    }

    CASE_CODE(STORE_PROPERTY_MONO) : {
      gab_value key = READ_CONSTANT;
      // Use the cache
      u64 prop_offset = *READ_QWORD;
      gab_obj_shape *cached_shape = GAB_VAL_TO_SHAPE(*READ_QWORD);

      gab_value value = PEEK();
      gab_value index = PEEK2();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found %V", index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      if (rec->shape != cached_shape) {
        prop_offset = gab_obj_shape_find(rec->shape, key);

        if (prop_offset == UINT64_MAX) {
          STORE_FRAME();
          return ERROR(GAB_MISSING_PROPERTY, "Missing '%V'", key);
        }

        // Transition to polymorphic
        WRITE_BYTE(PROP_CACHE_DIST, OP_STORE_PROPERTY_POLY);
      }

      gab_obj_record_set(VM(), rec, prop_offset, value);

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

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return ERROR(GAB_NOT_RECORD, "Found '%V'", index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);
      u64 prop_offset = gab_obj_shape_find(rec->shape, key);

      if (prop_offset == UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_MISSING_PROPERTY, "Missing '%V'", key);
      }

      gab_obj_record_set(VM(), rec, prop_offset, value);

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
      u8 n = READ_BYTE;

      gab_value tmp = PEEK();

      memcpy(TOP() - (n - 1), TOP() - n, (n - 1) * sizeof(gab_value));

      PEEK_N(n) = tmp;

      NEXT();
    }

    CASE_CODE(DROP) : {
      gab_value tmp = POP();

      DROP_N(READ_BYTE);
      PUSH(tmp);

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
      u8 n = READ_BYTE;
      gab_obj_string *acc =
          GAB_VAL_TO_STRING(gab_val_to_s(ENGINE(), PEEK_N(n)));

      for (u8 i = n - 1; i > 0; i--) {
        gab_obj_string *curr =
            GAB_VAL_TO_STRING(gab_val_to_s(ENGINE(), PEEK_N(i)));
        acc = gab_obj_string_concat(ENGINE(), acc, curr);
      }

      POP_N(n);

      PUSH(GAB_VAL_OBJ(acc));

      NEXT();
    }

    CASE_CODE(LOGICAL_AND) : {
      u16 offset = READ_SHORT;
      gab_value cond = PEEK();

      if (gab_val_falsey(cond)) {
        ip += offset;
      } else {
        DROP();
      }

      NEXT();
    }

    CASE_CODE(LOGICAL_OR) : {
      u16 offset = READ_SHORT;
      gab_value cond = PEEK();

      if (gab_val_falsey(cond)) {
        DROP();
      } else {
        ip += offset;
      }

      NEXT();
    }

    CASE_CODE(PUSH_NIL) : {
      PUSH(GAB_VAL_NIL());
      NEXT();
    }

    CASE_CODE(PUSH_UNDEFINED) : {
      PUSH(GAB_VAL_UNDEFINED());
      NEXT();
    }

    CASE_CODE(PUSH_TRUE) : {
      PUSH(GAB_VAL_BOOLEAN(true));
      NEXT();
    }

    CASE_CODE(PUSH_FALSE) : {
      PUSH(GAB_VAL_BOOLEAN(false));
      NEXT();
    }

    CASE_CODE(NEGATE) : {
      if (!GAB_VAL_IS_NUMBER(PEEK())) {
        STORE_FRAME();
        return ERROR(GAB_NOT_NUMERIC, "Found '%V'", PEEK());
      }

      gab_value new_value = GAB_VAL_NUMBER(-GAB_VAL_TO_NUMBER(POP()));

      PUSH(new_value);

      NEXT();
    }

    CASE_CODE(NOT) : {
      gab_value new_value = GAB_VAL_BOOLEAN(gab_val_falsey(POP()));
      PUSH(new_value);
      NEXT();
    }

    CASE_CODE(TYPE) : {
      PEEK() = gab_val_type(ENGINE(), PEEK());
      NEXT();
    }

    CASE_CODE(MATCH) : {
      gab_value test = POP();
      gab_value pattern = PEEK();
      if (test == pattern) {
        DROP();
        PUSH(GAB_VAL_BOOLEAN(true));
      } else {
        PUSH(GAB_VAL_BOOLEAN(false));
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
      u16 dist = READ_SHORT;
      ip += dist * !gab_val_falsey(POP());
      NEXT();
    }

    CASE_CODE(POP_JUMP_IF_FALSE) : {
      u16 dist = READ_SHORT;
      ip += dist * gab_val_falsey(POP());
      NEXT();
    }

    CASE_CODE(JUMP_IF_TRUE) : {
      u16 dist = READ_SHORT;
      ip += dist * !gab_val_falsey(PEEK());
      NEXT();
    }

    CASE_CODE(JUMP_IF_FALSE) : {
      u16 dist = READ_SHORT;
      ip += dist * gab_val_falsey(PEEK());
      NEXT();
    }

    CASE_CODE(JUMP) : {
      u16 dist = READ_SHORT;
      ip += dist;
      NEXT();
    }

    CASE_CODE(LOOP) : {
      u16 dist = READ_SHORT;
      ip -= dist;
      NEXT();
    }

    CASE_CODE(BLOCK) : {
      gab_obj_block_proto *p = READ_BLOCK_PROTOTYPE;

      gab_obj_block *blk = gab_obj_block_create(ENGINE(), p);

      for (int i = 0; i < p->nupvalues; i++) {
        u8 flags = p->upv_desc[i * 2];
        u8 index = p->upv_desc[i * 2 + 1];

        if (flags & fLOCAL) {
          blk->upvalues[i] = LOCAL(index);
        } else {
          blk->upvalues[i] = CLOSURE()->upvalues[index];
        }
      }

      gab_gc_iref_many(GC(), VM(), p->nupvalues, blk->upvalues);

      PUSH(GAB_VAL_OBJ(blk));

      gab_gc_dref(GC(), VM(), GAB_VAL_OBJ(blk));

      NEXT();
    }

    CASE_CODE(MESSAGE) : {
      gab_obj_block_proto *p = READ_BLOCK_PROTOTYPE;
      gab_obj_message *m = READ_MESSAGE;
      gab_value r = PEEK();

      u64 offset = gab_obj_message_find(m, r);

      if (offset != UINT64_MAX) {
        STORE_FRAME();
        return ERROR(GAB_IMPLEMENTATION_EXISTS, " Tried to implement %V for %V",
                     GAB_VAL_OBJ(m), r);
      }

      gab_obj_block *blk = gab_obj_block_create(ENGINE(), p);

      for (int i = 0; i < blk->nupvalues; i++) {
        u8 flags = p->upv_desc[i * 2];
        u8 index = p->upv_desc[i * 2 + 1];

        if (flags & fLOCAL) {
          assert(index < CLOSURE()->p->nlocals);
          blk->upvalues[i] = LOCAL(index);
        } else {
          assert(index < CLOSURE()->nupvalues);
          blk->upvalues[i] = CLOSURE()->upvalues[index];
        }
      }

      gab_gc_iref_many(GC(), VM(), blk->nupvalues, blk->upvalues);

      gab_gc_iref(GC(), VM(), r);

      gab_obj_message_insert(m, r, GAB_VAL_OBJ(blk));

      PEEK() = GAB_VAL_OBJ(m);

      NEXT();
    }

    CASE_CODE(PACK) : {
      u8 below = READ_BYTE;
      u8 above = READ_BYTE;

      u64 want = below + above;
      u64 have = VAR();
      u64 len = have - want;

      while (have < want)
        PUSH(GAB_VAL_NIL()), have++;

      gab_obj_shape *shape = gab_obj_shape_create_tuple(ENGINE(), VM(), len);

      gab_obj_record *rec =
          gab_obj_record_create(ENGINE(), VM(), shape, 1, TOP() - len - above);

      memcpy(TOP() - above, TOP() - above - 1, above * sizeof(gab_value));

      PEEK_N(above + 1) = GAB_VAL_OBJ(rec);

      gab_gc_dref(GC(), VM(), GAB_VAL_OBJ(rec));

      VAR() = want + 1;

      NEXT();
    }

    CASE_CODE(RECORD) : {
      u8 len = READ_BYTE;

      gab_obj_shape *shape =
          gab_obj_shape_create(ENGINE(), VM(), len, 2, TOP() - len * 2);

      gab_obj_record *rec = gab_obj_record_create(ENGINE(), VM(), shape, 2,
                                                  TOP() + 1 - (len * 2));

      DROP_N(len * 2);

      PUSH(GAB_VAL_OBJ(rec));

      gab_gc_dref(GC(), VM(), GAB_VAL_OBJ(rec));

      NEXT();
    }

    CASE_CODE(TUPLE) : {
      u8 len = parse_have(VM(), READ_BYTE);

      gab_obj_shape *shape = gab_obj_shape_create_tuple(ENGINE(), VM(), len);

      gab_obj_record *rec =
          gab_obj_record_create(ENGINE(), VM(), shape, 1, TOP() - len);

      DROP_N(len);

      PUSH(GAB_VAL_OBJ(rec));

      gab_gc_dref(GC(), VM(), GAB_VAL_OBJ(rec));

      NEXT();
    }
  }

  // This should be unreachable
  return a_gab_value_one(GAB_VAL_NIL());
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_PRIMITIVE
