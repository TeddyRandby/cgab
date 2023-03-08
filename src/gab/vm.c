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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
};

gab_value vm_error(gab_engine *gab, gab_vm *vm, u8 flags, gab_status e,
                   const char *help_fmt, ...) {

  u64 nframes = vm->fp - vm->fb;

  if (flags & GAB_FLAG_DUMP_ERROR) {
    for (;;) {
      gab_vm_frame *frame = vm->fb + nframes;

      s_i8 func_name =
          gab_obj_string_ref(GAB_VAL_TO_STRING(v_gab_constant_val_at(
              &frame->c->p->mod->constants, frame->c->p->mod->name)));

      gab_module *mod = frame->c->p->mod;

      if (!mod->source)
        break;

      u64 offset = frame->ip - mod->bytecode.data;

      u64 curr_row = v_u64_val_at(&mod->lines, offset);

      s_i8 curr_src = v_s_i8_val_at(&mod->source->source_lines, curr_row - 1);

      i8 *curr_src_start = curr_src.data;
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

      i8 *tok_start, *tok_end;
      tok_start = curr_token.data;
      tok_end = curr_token.data + curr_token.len;

      for (u8 i = 0; i < curr_under->len; i++) {
        if (curr_src_start + i >= tok_start && curr_src_start + i < tok_end)
          curr_under->data[i] = '^';
        else
          curr_under->data[i] = ' ';
      }

      if (nframes <= 1) {
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
                ANSI_COLOR_YELLOW "%s. " ANSI_COLOR_RESET ANSI_COLOR_GREEN,
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

      nframes--;
    }
  }

  if (flags & GAB_FLAG_EXIT_ON_PANIC) {
    exit(0);
  }

  return GAB_VAL_NIL();
}

gab_value gab_vm_panic(gab_engine *gab, gab_vm *vm, const char *msg) {
  return vm_error(gab, vm, GAB_FLAG_DUMP_ERROR, GAB_PANIC, msg);
}

void gab_vm_create(gab_vm *self, u8 flags, u8 argc, gab_value argv[argc]) {
  memset(self->sb, 0, sizeof(self->sb));

  self->fp = self->fb;
  self->sp = self->sb;
  self->flags = flags;
}

void gab_vm_stack_dump(gab_engine *gab, gab_vm *vm) {}

void gab_vm_frame_dump(gab_engine *gab, gab_vm *vm, u64 value) {
  u64 frame_count = vm->fp - vm->fb;

  if (value >= frame_count)
    return;

  gab_vm_frame *f = vm->fp - value;

  s_i8 func_name = gab_obj_string_ref(GAB_VAL_TO_STRING(
      v_gab_constant_val_at(&f->c->p->mod->constants, f->c->p->mod->name)));

  printf(ANSI_COLOR_GREEN " %03lu" ANSI_COLOR_RESET " closure:" ANSI_COLOR_CYAN
                          "%-20.*s" ANSI_COLOR_RESET "%d locals, %d upvalues\n",
         frame_count - value, (i32)func_name.len, func_name.data,
         f->c->p->nlocals, f->c->p->nupvalues);

  for (i32 i = f->c->p->nslots - 1; i >= 0; i--) {
    if (i < f->c->p->nlocals)
      printf(ANSI_COLOR_MAGENTA "%4i " ANSI_COLOR_RESET, i);
    else
      printf(ANSI_COLOR_YELLOW "%4i " ANSI_COLOR_RESET, i);

    gab_val_dump(stdout, f->slots[i]);
    printf("\n");
  }
}

static inline i32 parse_have(gab_vm *vm, u8 have) {
  if (have & FLAG_VAR_EXP)
    return *vm->sp + (have >> 1);
  else
    return have >> 1;
}

static inline gab_value *trim_return(gab_value *from, gab_value *to, u8 have,
                                     u8 want) {
  u8 nulls = have == 0;

  if ((have != want) && (want != VAR_EXP)) {
    if (have > want)
      have = want;
    else
      nulls = want - have;
  }

  const u8 got = have + nulls;

  while (have--)
    *to++ = *from++;

  while (nulls--)
    *to++ = GAB_VAL_NIL();

  *to = got;
  return to;
}

static inline boolean has_callspace(gab_vm *vm, u8 space_needed) {
  if (vm->fp - vm->fb + 1 >= FRAMES_MAX) {
    return false;
  }

  if (vm->sp - vm->sb + space_needed >= STACK_MAX) {
    return false;
  }

  return true;
}

static inline boolean call_suspense(gab_vm *vm, gab_obj_suspense *e, u8 arity,
                                    u8 want) {
  i16 space_needed = e->len - e->have - arity;

  if (space_needed > 0 && !has_callspace(vm, space_needed))
    return false;

  vm->fp++;
  vm->fp->c = e->c;
  vm->fp->ip = e->c->p->mod->bytecode.data + e->offset;
  vm->fp->want = want;
  vm->fp->slots = vm->sp - arity - 1;

  gab_value *from = vm->sp - arity;
  gab_value *to = vm->fp->slots + e->len - e->have;

  vm->sp = trim_return(from, to, arity, e->want);
  memcpy(vm->fp->slots, e->frame, (e->len - e->have) * sizeof(gab_value));

  return true;
}

static inline boolean call_block_var(gab_engine *gab, gab_vm *vm,
                                     gab_obj_block *c, u8 have, u8 want) {
  vm->fp++;
  vm->fp->c = c;
  vm->fp->ip = c->p->mod->bytecode.data;
  vm->fp->want = want;

  u8 size = have - c->p->narguments;

  have = c->p->narguments + 1;

  gab_obj_shape *shape = gab_obj_shape_create_tuple(gab, vm, size);

  gab_value args =
      GAB_VAL_OBJ(gab_obj_record_create(gab, vm, shape, 1, vm->sp - size));

  vm->sp -= size;

  *vm->sp++ = args;

  gab_gc_dref(gab, vm, args);

  vm->fp->slots = vm->sp - have - 1;

  for (u8 i = c->p->narguments + 1; i < c->p->nlocals; i++)
    *vm->sp++ = GAB_VAL_NIL();

  return true;
}

static inline boolean call_block(gab_vm *vm, gab_obj_block *c, u8 have,
                                 u8 want) {
  if (!has_callspace(vm, c->p->nslots - c->p->narguments - 1)) {
    return false;
  }

  vm->fp++;
  vm->fp->c = c;
  vm->fp->ip = c->p->mod->bytecode.data;
  vm->fp->want = want;

  while (have < c->p->narguments)
    *vm->sp++ = GAB_VAL_NIL(), have++;

  while (have > c->p->narguments)
    vm->sp--, have--;

  vm->fp->slots = vm->sp - have - 1;

  for (u8 i = c->p->narguments + 1; i < c->p->nlocals; i++)
    *vm->sp++ = GAB_VAL_NIL();

  return true;
}

static inline void call_builtin(gab_engine *gab, gab_vm *vm, gab_obj_builtin *b,
                                u8 arity, u8 want, boolean is_message) {
  // Only pass in the extra "self" argument if this is a message.
  gab_value result =
      (*b->function)(gab, vm, arity + is_message, vm->sp - arity - is_message);

  // There is always an extra to trim bc of the receiver or callee.
  vm->sp = trim_return(&result, vm->sp - arity - 1, 1, want);
}

gab_value gab_vm_run(gab_engine *gab, gab_value main, u8 flags, u8 argc,
                     gab_value argv[argc]) {

#if GAB_LOG_EXECUTION
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
    return vm_error(ENGINE(), VM(), flags, GAB_NOT_NUMERIC, "Found %V",        \
                    PEEK());                                                   \
  }                                                                            \
  operation_type b = GAB_VAL_TO_NUMBER(POP());                                 \
  operation_type a = GAB_VAL_TO_NUMBER(POP());                                 \
  PUSH(value_type(a operation b));                                             \
  VAR() = 1;

/*
  Lots of helper macros.
*/
#define ENGINE() (gab)
#define GC() (&ENGINE()->gc)
#define VM() (&vm)
#define INSTR() (instr)
#define FRAME() (VM()->fp)
#define CLOSURE() (FRAME()->c)
#define MODULE() (CLOSURE()->p->mod)
#define IP() (ip)
#define TOP() (VM()->sp)
#define VAR() (*TOP())
#define SLOTS() (FRAME()->slots)
#define LOCAL(i) (SLOTS()[i])
#define UPVALUE(i) (CLOSURE()->upvalues[i])
#define MOD_CONSTANT(k) (v_gab_constant_val_at(&MODULE()->constants, k))

#if GAB_DEBUG_VM
#define PUSH(value)                                                            \
  ({                                                                           \
    if (TOP() > (SLOTS() + CLOSURE()->p->nslots)) {                            \
      fprintf(stderr, "Stack exceeded frame. %lu passed\n",                    \
              TOP() - SLOTS() - CLOSURE()->p->nslots);                         \
      dump_frame(SLOTS(), TOP(), "stack");                                     \
    }                                                                          \
    *TOP()++ = value;                                                          \
  })
#else
#define PUSH(value) (*TOP()++ = value)
#endif
#define POP() (*(--TOP()))
#define DROP() (TOP()--)
#define POP_N(n) (TOP() -= n)
#define DROP_N(n) (TOP() -= n)
#define PEEK() (*(TOP() - 1))
#define PEEK2() (*(TOP() - 2))
#define PEEK_N(n) (*(TOP() - (n)))

#define WRITE_BYTE(dist, n) (*(IP() - dist) = n)

#define WRITE_INLINEBYTE(n) (*IP()++ = n)
#define WRITE_INLINESHORT(n)                                                   \
  (IP() += 2, IP()[-2] = (n >> 8) & 0xFF, IP()[-1] = n & 0xFF)
#define WRITE_INLINEQWORD(n) (IP() += 8, *((u64 *)(IP() - 8)) = n)

#define SKIP_BYTE (IP()++)
#define SKIP_SHORT (IP() += 2)
#define SKIP_QWORD (IP() += 8)

#define READ_BYTE (*IP()++)
#define READ_SHORT (IP() += 2, (((u16)IP()[-2] << 8) | IP()[-1]))
#define READ_QWORD (IP() += 8, (gab_value *)(IP() - 8))

#define READ_CONSTANT (MOD_CONSTANT(READ_SHORT))
#define READ_STRING (GAB_VAL_TO_STRING(READ_CONSTANT))
#define READ_MESSAGE (GAB_VAL_TO_MESSAGE(READ_CONSTANT))
#define READ_PROTOTYPE (GAB_VAL_TO_PROTOTYPE(READ_CONSTANT))

#define STORE_FRAME() ({ FRAME()->ip = IP(); })
#define LOAD_FRAME() ({ IP() = FRAME()->ip; })

#define SEND_CACHE_DIST 22

  /*
   ----------- BEGIN RUN BODY -----------
  */
  gab_vm vm;
  gab_vm_create(&vm, flags, argc, argv);

  register u8 instr = OP_NOP;
  register u8 *ip = NULL;

  PUSH(main);
  for (u8 i = 0; i < argc; i++)
    PUSH(argv[i]);

  if (GAB_VAL_IS_BLOCK(main)) {
    gab_obj_block *b = GAB_VAL_TO_BLOCK(main);

    if (!call_block(VM(), b, argc, 1))
      return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");
  } else if (GAB_VAL_IS_SUSPENSE(main)) {
    gab_obj_suspense *s = GAB_VAL_TO_SUSPENSE(main);

    if (!call_suspense(VM(), s, argc, 1))
      return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");
  } else {
    return GAB_VAL_NIL();
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

      gab_value spec = gab_obj_message_read(msg, type);

      u64 offset;
      if (GAB_VAL_IS_UNDEFINED(spec)) {
        spec = gab_obj_message_read(msg, GAB_VAL_UNDEFINED());

        if (GAB_VAL_IS_UNDEFINED(spec)) {
          STORE_FRAME();
          return vm_error(ENGINE(), VM(), flags, GAB_IMPLEMENTATION_MISSING,
                          "Could not send %V to %V", GAB_VAL_OBJ(msg), type);
        }

        offset = gab_obj_message_find(msg, GAB_VAL_UNDEFINED());
      } else {
        offset = gab_obj_message_find(msg, type);
      }

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
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_CALLABLE, "Found %V",
                        spec);
      }

      IP() -= SEND_CACHE_DIST;

      NEXT();
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

      if (blk->p->var) {
        if (!call_block_var(ENGINE(), VM(), blk, have, want))
          return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");
      } else {
        if (!call_block(VM(), blk, have, want))
          return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");
      }

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

      if (blk->p->var) {
        if (!call_block_var(ENGINE(), VM(), blk, have, want))
          return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");
      } else {
        if (!call_block(VM(), blk, have, want))
          return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");
      }

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
        return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");

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
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_STRING, "Found %V",
                        PEEK());
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

    CASE_CODE(SEND_PRIMITIVE_STORE_ANA) : {
      SKIP_SHORT;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_QWORD;
      SKIP_QWORD;

      gab_value key = PEEK2();
      gab_value index = PEEK_N(3);

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD,
                        "Tried to store property %V on %V", key, index);
      }
      gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

      u16 prop_offset = gab_obj_shape_find(obj->shape, key);

      if (prop_offset == UINT16_MAX) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_MISSING_PROPERTY, "Found %V",
                        index);
      }

      WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PRIMITIVE_STORE_MONO);

      IP() -= SEND_CACHE_DIST;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_STORE_MONO) : {
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

      u16 prop_offset = gab_obj_shape_find(GAB_VAL_TO_SHAPE(type), key);

      if ((cached_type != type) | (version != msg->version) |
          (prop_offset == UINT16_MAX)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_ANA);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

      gab_obj_record_set(ENGINE(), VM(), obj, prop_offset, value);

      DROP_N(3);

      PUSH(value);

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(SEND_PRIMITIVE_LOAD_ANA) : {
      SKIP_SHORT;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_BYTE;
      SKIP_QWORD;
      SKIP_QWORD;

      gab_value index = PEEK2();

      if (GAB_VAL_IS_RECORD(index)) {
        WRITE_BYTE(SEND_CACHE_DIST, OP_SEND_PRIMITIVE_LOAD_MONO);
        IP() -= SEND_CACHE_DIST;
        NEXT();
      }

      STORE_FRAME();
      return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD, "Found %V", index);
    }

    CASE_CODE(SEND_PRIMITIVE_LOAD_MONO) : {
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

      if (prop_offset == UINT16_MAX)
        PUSH(GAB_VAL_NIL());
      else
        PUSH(gab_obj_record_get(obj, prop_offset));

      VAR() = 1;

      NEXT();
    }

    CASE_CODE(ITER) : {
      u16 dist = READ_SHORT;
      u8 want = READ_BYTE;
      u8 start = READ_BYTE;

      u8 have = VAR();

      gab_value eff = PEEK();

      if (!GAB_VAL_IS_SUSPENSE(eff)) {

        DROP_N(have - 1);

        // Account for the two bytes we read already
        ip += dist - 2;

        NEXT();
      }

      // Hide the effect but pretending to have one less
      // Don't update the top of the stack, just trim into the locals
      // where we expect.
      trim_return(TOP() - have, SLOTS() + start, have - 1, want);

      // Update the iterator
      LOCAL(start + want) = eff;

      DROP_N(have);

      NEXT();
    }

    CASE_CODE(NEXT) : {
      u8 iter = READ_BYTE;

      PUSH(LOCAL(iter));

      STORE_FRAME();

      if (!call_suspense(VM(), GAB_VAL_TO_SUSPENSE(PEEK()), 0, VAR_EXP))
        return vm_error(ENGINE(), VM(), flags, GAB_OVERFLOW, "");

      LOAD_FRAME();

      NEXT();
    }

    {

      u8 have;

      {
        u8 want;

        CASE_CODE(YIELD) : {
          have = parse_have(VM(), READ_BYTE);
          want = READ_BYTE;

          u64 frame_len = TOP() - SLOTS();

          gab_value sus = GAB_VAL_OBJ(gab_obj_suspense_create(
              ENGINE(), VM(), CLOSURE(), IP() - MODULE()->bytecode.data, have,
              want, frame_len, SLOTS()));

          PUSH(sus);

          gab_gc_dref(ENGINE(), VM(), sus);

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
        gab_value result = POP();
        gab_gc_iref(ENGINE(), VM(), result);
        return result;
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
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD, "Found %V",
                        index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);
      u16 prop_offset = gab_obj_shape_find(rec->shape, key);

      // Transition state and store in the ache
      WRITE_INLINESHORT(prop_offset);
      WRITE_INLINEQWORD(GAB_VAL_OBJ(rec->shape));
      WRITE_BYTE(13, OP_LOAD_PROPERTY_MONO);

      IP() -= 13;

      NEXT();
    }

    CASE_CODE(LOAD_PROPERTY_MONO) : {
      gab_value key = READ_CONSTANT;
      u16 prop_offset = READ_SHORT;
      gab_obj_shape *cached_shape = GAB_VAL_TO_SHAPE(*READ_QWORD);

      gab_value index = PEEK();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD,
                        "Tried to load property %V on %V", key, index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      if (cached_shape != rec->shape) {
        WRITE_BYTE(13, OP_LOAD_PROPERTY_POLY);
        IP() -= 13;
        NEXT();
      }

      DROP();

      if (prop_offset == UINT16_MAX)
        PUSH(GAB_VAL_NIL());
      else
        PUSH(gab_obj_record_get(rec, prop_offset));

      NEXT();
    }

    CASE_CODE(LOAD_PROPERTY_POLY) : {
      gab_value key = READ_CONSTANT;
      SKIP_SHORT;
      SKIP_QWORD;

      gab_value index = PEEK();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD, "Found %V",
                        index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      u16 prop_offset = gab_obj_shape_find(rec->shape, key);

      DROP();

      if (prop_offset == UINT16_MAX)
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
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD, "Found %V",
                        index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      u16 prop_offset = gab_obj_shape_find(rec->shape, key);

      if (prop_offset == UINT16_MAX) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_MISSING_PROPERTY,
                        "%V has no property %V", index, key);
      }

      // Write to the cache and transition to monomorphic
      WRITE_INLINESHORT(prop_offset);
      WRITE_INLINEQWORD((u64)rec->shape);
      WRITE_BYTE(13, OP_STORE_PROPERTY_MONO);

      IP() -= 13;

      NEXT();
    }

    CASE_CODE(STORE_PROPERTY_MONO) : {
      gab_value key = READ_CONSTANT;
      // Use the cache
      u16 prop_offset = READ_SHORT;
      gab_obj_shape *cached_shape = GAB_VAL_TO_SHAPE(*READ_QWORD);

      gab_value value = PEEK();
      gab_value index = PEEK2();

      if (!GAB_VAL_IS_RECORD(index)) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD, "Found %V",
                        index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);

      if (rec->shape != cached_shape) {

        prop_offset = gab_obj_shape_find(rec->shape, key);

        if (prop_offset == UINT16_MAX) {
          STORE_FRAME();
          return vm_error(ENGINE(), VM(), flags, GAB_MISSING_PROPERTY,
                          "%V has no property %V", index, key);
        }

        // Transition to polymorphic
        WRITE_BYTE(13, OP_STORE_PROPERTY_POLY);
      }

      gab_obj_record_set(ENGINE(), VM(), rec, prop_offset, value);

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
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_RECORD, "Found %V",
                        index);
      }

      gab_obj_record *rec = GAB_VAL_TO_RECORD(index);
      u16 prop_offset = gab_obj_shape_find(rec->shape, key);

      if (prop_offset == UINT16_MAX) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_MISSING_PROPERTY,
                        "%V has no property %V", index, key);
      }

      gab_obj_record_set(ENGINE(), VM(), rec, prop_offset, value);

      DROP_N(2);

      PUSH(value);

      NEXT();
    }

    CASE_CODE(NOP) : {
      exit(1);
      NEXT();
    }

    CASE_CODE(CONSTANT) : {
      PUSH(READ_CONSTANT);
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
          GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK_N(n)));

      for (u8 i = n - 1; i > 0; i--) {
        gab_obj_string *curr =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK_N(i)));
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
        return vm_error(ENGINE(), VM(), flags, GAB_NOT_NUMERIC, "Found %V",
                        PEEK());
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

    CASE_CODE(POP) : {
      DROP();
      NEXT();
    }

    CASE_CODE(POP_N) : {
      DROP_N(READ_BYTE);
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
      gab_obj_prototype *p = READ_PROTOTYPE;

      gab_obj_block *blk = gab_obj_block_create(ENGINE(), p);

      for (int i = 0; i < p->nupvalues; i++) {
        u8 flags = p->upv_desc[i * 2];
        u8 index = p->upv_desc[i * 2 + 1];

        if (flags & GAB_VARIABLE_FLAG_LOCAL) {
          blk->upvalues[i] = LOCAL(index);
        } else {
          blk->upvalues[i] = CLOSURE()->upvalues[index];
        }
      }

      gab_gc_iref_many(ENGINE(), VM(), p->nupvalues, blk->upvalues);

      PUSH(GAB_VAL_OBJ(blk));

      gab_gc_dref(ENGINE(), VM(), GAB_VAL_OBJ(blk));

      NEXT();
    }

    CASE_CODE(MESSAGE) : {
      gab_obj_prototype *p = READ_PROTOTYPE;
      gab_obj_message *m = READ_MESSAGE;
      gab_value r = PEEK();

      u64 offset = gab_obj_message_find(m, r);

      if (offset != UINT64_MAX) {
        STORE_FRAME();
        return vm_error(ENGINE(), VM(), flags, GAB_IMPLEMENTATION_EXISTS,
                        " Tried to implement %V for %V", GAB_VAL_OBJ(m), r);
      }

      gab_obj_block *blk = gab_obj_block_create(ENGINE(), p);

      for (int i = 0; i < blk->nupvalues; i++) {
        u8 flags = p->upv_desc[i * 2];
        u8 index = p->upv_desc[i * 2 + 1];

        if (flags & GAB_VARIABLE_FLAG_LOCAL) {
          blk->upvalues[i] = LOCAL(index);
        } else {
          blk->upvalues[i] = CLOSURE()->upvalues[index];
        }
      }

      gab_gc_iref_many(ENGINE(), VM(), blk->nupvalues, blk->upvalues);

      gab_gc_iref(ENGINE(), VM(), r);

      gab_obj_message_insert(m, r, GAB_VAL_OBJ(blk));

      PEEK() = GAB_VAL_OBJ(m);

      NEXT();
    }

    {
      gab_obj_shape *shape;
      u8 len;

      CASE_CODE(RECORD_DEF) : {
        gab_value name = READ_CONSTANT;

        len = READ_BYTE;

        shape = gab_obj_shape_create(ENGINE(), VM(), len, 2, TOP() - len * 2);

        shape->name = name;

        goto complete_record;
      }

      CASE_CODE(RECORD) : {
        len = READ_BYTE;

        shape = gab_obj_shape_create(ENGINE(), VM(), len, 2, TOP() - len * 2);

        goto complete_record;
      }

    complete_record : {
      gab_obj_record *rec = gab_obj_record_create(ENGINE(), VM(), shape, 2,
                                                  TOP() + 1 - (len * 2));

      DROP_N(len * 2);

      PUSH(GAB_VAL_OBJ(rec));

      gab_gc_dref(ENGINE(), VM(), GAB_VAL_OBJ(rec));

      NEXT();
    }
    }

    CASE_CODE(TUPLE) : {
      u8 len = parse_have(VM(), READ_BYTE);

      gab_obj_shape *shape = gab_obj_shape_create_tuple(ENGINE(), VM(), len);

      gab_obj_record *rec =
          gab_obj_record_create(ENGINE(), VM(), shape, 1, TOP() - len);

      DROP_N(len);

      PUSH(GAB_VAL_OBJ(rec));

      gab_gc_dref(ENGINE(), VM(), GAB_VAL_OBJ(rec));

      NEXT();
    }
  }
  // This should be unreachable
  return GAB_VAL_NIL();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_PRIMITIVE
