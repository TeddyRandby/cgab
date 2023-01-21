#include "include/char.h"
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
#include <string.h>

static const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
};

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
gab_value vm_error(gab_vm *vm, gab_status e, const char *help_fmt, ...) {
  if (vm->flags & GAB_FLAG_DUMP_ERROR) {
    gab_call_frame *frame = vm->call_stack + 1;

    while (frame <= vm->frame) {
      s_i8 func_name = frame->c->p->name;
      gab_module *mod = frame->c->p->mod;

      u64 offset = frame->ip - mod->bytecode.data - 1;

      u64 curr_row = v_u64_val_at(&mod->lines, offset);

      // Row numbers start at one, so the index is one less.
      s_i8 curr_src = v_s_i8_val_at(mod->source_lines, curr_row - 1);

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

      if (frame == vm->frame) {

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
      } else {
        fprintf(stderr,
                "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET "] Called at:"
                "\n\t  " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%.*s"
                "\n\t       " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET "\n",
                (i32)func_name.len, func_name.data, curr_row, curr_src_len,
                curr_src_start, (i32)curr_under->len, curr_under->data);
      }

      frame++;
    }
  }

  if (vm->flags & GAB_FLAG_PANIC_ON_FAIL) {
    exit(0);
  }

  return GAB_VAL_NIL();
}

void gab_vm_panic(gab_engine *gab, gab_vm *vm, const char *msg) {
  vm_error(vm, GAB_PANIC, msg);

  exit(0);
}

void gab_vm_create(gab_vm *self, u8 flags, u8 argc, gab_value argv[argc]) {
  self->open_upvalues = NULL;
  memset(self->stack, 0, sizeof(self->stack));

  self->frame = self->call_stack;
  self->top = self->stack;
  self->flags = flags;
  self->argc = argc;
  self->argv = argv;
}

void dump_frame(gab_value *slots, gab_value *top, const char *name) {
  printf("Frame at %s:\n-----------------\n", name);
  gab_value *tracker = top - 1;
  while (tracker >= slots) {
    printf("|%lu|", tracker - slots);
    gab_val_dump(*tracker--);
    printf("\n");
  }
  printf("---------------\n");
};

void dump_stack(gab_vm *vm, gab_value *top, const char *name) {
  printf("Stack at %s:\n-----------------\n", name);
  gab_value *tracker = top - 1;
  while (tracker >= vm->stack) {
    printf("|%lu|", tracker - vm->stack);
    gab_val_dump(*tracker--);
    printf("\n");
  }
  printf("---------------\n");
}

static inline gab_value *trim_return(gab_vm *vm, gab_value *from, gab_value *to,
                                     u8 have, u8 want) {
  u8 nulls = have == 0;

  if ((have != want) && (want != VAR_RET)) {
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

static inline void call_effect(gab_vm *vm, gab_obj_effect *e, u8 arity,
                               u8 want) {
  vm->frame++;
  vm->frame->c = e->c;
  vm->frame->ip = e->ip;
  vm->frame->want = want;
  vm->frame->slots = vm->top - arity - 1;

  gab_value *from = vm->top - arity;
  gab_value *to = vm->frame->slots + e->len - e->have;

  vm->top = trim_return(vm, from, to, arity, e->want);
  memcpy(vm->frame->slots, e->frame, (e->len - e->have) * sizeof(gab_value));
}

static inline void call_closure(gab_vm *vm, gab_obj_closure *c, u8 arity,
                                u8 want) {
  while (arity < c->p->narguments)
    *vm->top++ = GAB_VAL_NIL(), arity++;

  while (arity > c->p->narguments)
    vm->top--, arity--;

  vm->frame++;
  vm->frame->c = c;
  vm->frame->ip = c->p->mod->bytecode.data + c->p->offset;
  vm->frame->slots = vm->top - arity - 1;
  vm->frame->want = want;
  vm->top += c->p->nlocals;
}

static inline void call_builtin(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                gab_obj_builtin *b, u8 arity, u8 want) {
  gab_value result = (*b->function)(gab, vm, arity, vm->top - arity);

  vm->top -= arity;

  vm->top = trim_return(vm, &result, vm->top, 1, want);

  gab_gc_dref(gab, vm, gc, result);
}

static inline gab_value capture_upvalue(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                        gab_value *local) {
  gab_obj_upvalue *prev_upvalue = NULL;
  gab_obj_upvalue *upvalue = vm->open_upvalues;

  while (upvalue != NULL && upvalue->data > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->data == local) {
    // Capture this upvalue with a new referemce
    gab_gc_iref(gab, vm, gc, GAB_VAL_OBJ(upvalue));
    return GAB_VAL_OBJ(upvalue);
  }

  gab_obj_upvalue *new_upvalue = gab_obj_upvalue_create(local);

  new_upvalue->next = upvalue;
  if (prev_upvalue == NULL) {
    vm->open_upvalues = new_upvalue;
  } else {
    prev_upvalue->next = new_upvalue;
  }
  return GAB_VAL_OBJ(new_upvalue);
}

static inline void close_upvalue(gab_engine *gab, gab_gc *gc, gab_vm *vm,
                                 gab_value *local) {
  while (vm->open_upvalues != NULL && vm->open_upvalues->data >= local) {
    gab_obj_upvalue *upvalue = vm->open_upvalues;

    upvalue->closed = *upvalue->data;
    gab_gc_iref(gab, vm, gc, upvalue->closed);

    upvalue->data = &upvalue->closed;

    vm->open_upvalues = upvalue->next;
  }
}

gab_value gab_vm_run(gab_engine *gab, gab_module *mod, u8 flags, u8 argc,
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

  /*
   * We do a bitwise or because it is slightly faster than a logical or.
   * It is faster because there is no reliable branch to predict,
   * and short circuiting can't help us.
   */
#define BINARY_OPERATION(value_type, operation_type, operation)                \
  if (!(GAB_VAL_IS_NUMBER(PEEK()) | GAB_VAL_IS_NUMBER(PEEK2()))) {             \
    STORE_FRAME();                                                             \
    gab_obj_string *a =                                                        \
        GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK()));                \
                                                                               \
    gab_obj_string *b =                                                        \
        GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK2()));               \
                                                                               \
    return vm_error(VM(), GAB_NOT_NUMERIC,                                     \
                    "Tried to " #operation " %.*s and %.*s", (i32)b->len,      \
                    b->data, (i32)a->len, a->data);                            \
  }                                                                            \
  operation_type b = GAB_VAL_TO_NUMBER(POP());                                 \
  operation_type a = GAB_VAL_TO_NUMBER(POP());                                 \
  PUSH(value_type(a operation b));

/*
  Lots of helper macros.
*/
#define VM() (&vm)
#define ENGINE() (gab)
#define GC() (&ENGINE()->gc)
#define INSTR() (instr)
#define FRAME() (vm.frame)
#define CLOSURE() (FRAME()->c)
#define MODULE() (CLOSURE()->p->mod)
#define IP() (ip)
#define TOP() (VM()->top)
#define SLOTS() (FRAME()->slots)
#define LOCAL(i) (SLOTS()[i])
#define UPVALUE(i) (*(GAB_VAL_TO_UPVALUE(CLOSURE()->upvalues[i])->data))
#define CONST_UPVALUE(i) (CLOSURE()->upvalues[i])
#define MOD_CONSTANT(k) (d_gab_constant_ikey(&MODULE()->constants, k))

#define PUSH(value) (*TOP()++ = value)
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
#define READ_QWORD(type) (IP() += 8, (type *)(IP() - 8))

#define READ_CONSTANT (MOD_CONSTANT(READ_SHORT))
#define READ_STRING (GAB_VAL_TO_STRING(READ_CONSTANT))
#define READ_MESSAGE (GAB_VAL_TO_MESSAGE(READ_CONSTANT))
#define READ_PROTOTYPE (GAB_VAL_TO_PROTOTYPE(READ_CONSTANT))

#define STORE_FRAME() ({ FRAME()->ip = IP(); })
#define LOAD_FRAME() ({ IP() = FRAME()->ip; })

  /*
   ----------- BEGIN RUN BODY -----------
  */

  gab_vm vm;
  gab_vm_create(VM(), flags, argc, argv);

  gab_obj_closure *main_c =
      GAB_VAL_TO_CLOSURE(d_gab_constant_ikey(&mod->constants, mod->main));

  FRAME()->c = main_c;

  register u8 instr;
  register u8 *ip = main_c->p->mod->bytecode.data;

  PUSH(GAB_VAL_OBJ(main_c));

  for (u8 i = 0; i < argc; i++) {
    PUSH(argv[i]);
  }

  STORE_FRAME();
  call_closure(VM(), main_c, main_c->p->narguments, 1);
  LOAD_FRAME();

  NEXT();

  LOOP() {

    {
      u8 arity, want;

      CASE_CODE(DYNSEND) : {
        arity = READ_BYTE;
        want = READ_BYTE;

        goto complete_dynsend;
      }

      CASE_CODE(VARDYNSEND) : {
        arity = *TOP() + READ_BYTE;
        want = READ_BYTE;

        goto complete_dynsend;
      }

    complete_dynsend : {
      gab_value callee = PEEK_N(arity + 1);

      STORE_FRAME();
      if (GAB_VAL_IS_CLOSURE(callee)) {
        call_closure(VM(), GAB_VAL_TO_CLOSURE(callee), arity, want);
      } else if (GAB_VAL_IS_BUILTIN(callee)) {
        call_builtin(ENGINE(), VM(), GC(), GAB_VAL_TO_BUILTIN(callee), arity,
                     want);
      } else if (GAB_VAL_IS_EFFECT(callee)) {
        call_effect(VM(), GAB_VAL_TO_EFFECT(callee), arity, want);
      } else {
        gab_obj_string *a =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), callee));
        return vm_error(VM(), GAB_NOT_CALLABLE, "Tried to call %.*s",
                        (i32)a->len, a->data);
      }

      LOAD_FRAME();
      NEXT();
    }
    }

    {
      gab_obj_message *func;
      u8 arity, want;

      CASE_CODE(VARSEND_ANA) : {
        func = READ_MESSAGE;
        arity = *TOP() + READ_BYTE;
        want = READ_BYTE;

        goto complete_send_ana;
      }

      CASE_CODE(SEND_ANA) : {
        func = READ_MESSAGE;
        arity = READ_BYTE;
        want = READ_BYTE;

        goto complete_send_ana;
      }

    complete_send_ana : {
      gab_value receiver = PEEK_N(arity + 1);

      gab_value type = gab_typeof(ENGINE(), receiver);

      gab_value spec = gab_obj_message_read(func, type);

      u16 offset;
      if (GAB_VAL_IS_NIL(spec)) {
        spec = gab_obj_message_read(func, GAB_VAL_UNDEFINED());

        if (GAB_VAL_IS_NIL(spec)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), receiver));
          return vm_error(VM(), GAB_NOT_CALLABLE,
                          "No specialization for receiver '%.*s'", (i32)a->len,
                          a->data);
        }

        offset = gab_obj_message_find(func, GAB_VAL_UNDEFINED());
      } else {
        offset = gab_obj_message_find(func, type);
      }

      WRITE_INLINEBYTE(func->version);
      WRITE_INLINESHORT(offset);
      WRITE_INLINEQWORD(type);

      STORE_FRAME();
      if (GAB_VAL_IS_CLOSURE(spec)) {
        WRITE_BYTE(16, instr + 1);

        call_closure(VM(), GAB_VAL_TO_CLOSURE(spec), arity, want);
        LOAD_FRAME();

        NEXT();
      } else if (GAB_VAL_IS_BUILTIN(spec)) {
        WRITE_BYTE(16, instr + 2);

        call_builtin(ENGINE(), VM(), GC(), GAB_VAL_TO_BUILTIN(spec), arity + 1,
                     want);

        NEXT();
      } else {
        return vm_error(VM(), GAB_NOT_CALLABLE, "");
      }
    }
    }

    {
      gab_obj_message *func;
      u8 arity, want, version;
      u16 offset;
      gab_value cached_type;

      CASE_CODE(VARSEND_MONO_CLOSURE) : {
        func = READ_MESSAGE;
        arity = *TOP() + READ_BYTE;
        want = READ_BYTE;
        version = READ_BYTE;
        offset = READ_SHORT;
        cached_type = *READ_QWORD(gab_value);

        goto complete_send_mono_closure;
      }

      CASE_CODE(SEND_MONO_CLOSURE) : {
        func = READ_MESSAGE;
        arity = READ_BYTE;
        want = READ_BYTE;
        version = READ_BYTE;
        offset = READ_SHORT;
        cached_type = *READ_QWORD(gab_value);
      }

    complete_send_mono_closure : {
      gab_value receiver = PEEK_N(arity + 1);

      gab_value type = gab_typeof(ENGINE(), receiver);

      if ((cached_type != type) | (version != func->version)) {
        // Revert to anamorphic
        WRITE_BYTE(16, instr - 1);

        offset = gab_obj_message_find(func, type);

        if (offset == UINT16_MAX) {

          offset = gab_obj_message_find(func, GAB_VAL_NIL());

          if (offset == UINT16_MAX) {
            STORE_FRAME();
            gab_obj_string *a =
                GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), receiver));
            return vm_error(VM(), GAB_NOT_CALLABLE,
                            "No specialization for receiver '%.*s'",
                            (i32)a->len, a->data);
          }
        }

        gab_value spec = gab_obj_message_get(func, offset);

        STORE_FRAME();
        if (GAB_VAL_IS_CLOSURE(spec)) {
          call_closure(VM(), GAB_VAL_TO_CLOSURE(spec), arity, want);
          LOAD_FRAME();

          NEXT();
        } else if (GAB_VAL_IS_BUILTIN(spec)) {
          call_builtin(ENGINE(), VM(), GC(), GAB_VAL_TO_BUILTIN(spec),
                       arity + 1, want);

          NEXT();
        } else {
          return vm_error(VM(), GAB_NOT_CALLABLE, "");
        }
      }

      gab_value spec = gab_obj_message_get(func, offset);

      STORE_FRAME();
      call_closure(VM(), GAB_VAL_TO_CLOSURE(spec), arity, want);
      LOAD_FRAME();

      NEXT();
    }

      CASE_CODE(SEND_MONO_BUILTIN) : {
        func = READ_MESSAGE;
        arity = READ_BYTE;
        want = READ_BYTE;
        version = READ_BYTE;
        offset = READ_SHORT;
        cached_type = *READ_QWORD(gab_value);

        goto complete_send_mono_builtin;
      }

      CASE_CODE(VARSEND_MONO_BUILTIN) : {
        func = READ_MESSAGE;
        arity = *TOP() + READ_BYTE;
        want = READ_BYTE;
        version = READ_BYTE;
        offset = READ_SHORT;
        cached_type = *READ_QWORD(gab_value);

        goto complete_send_mono_builtin;
      }

    complete_send_mono_builtin : {
      gab_value receiver = PEEK_N(arity + 1);

      gab_value type = gab_typeof(ENGINE(), receiver);

      if ((cached_type != type) | (version != func->version)) {
        // Revert to anamorphic
        WRITE_BYTE(16, instr - 2);

        offset = gab_obj_message_find(func, type);

        if (offset == UINT16_MAX) {
          offset = gab_obj_message_find(func, GAB_VAL_NIL());

          if (offset == UINT16_MAX) {
            STORE_FRAME();
            gab_obj_string *a =
                GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), receiver));
            return vm_error(VM(), GAB_NOT_CALLABLE,
                            "No specialization for receiver '%.*s'",
                            (i32)a->len, a->data);
          }
        }

        gab_value spec = gab_obj_message_get(func, offset);

        STORE_FRAME();
        if (GAB_VAL_IS_CLOSURE(spec)) {
          call_closure(VM(), GAB_VAL_TO_CLOSURE(spec), arity, want);
          LOAD_FRAME();

          NEXT();
        } else if (GAB_VAL_IS_BUILTIN(spec)) {
          call_builtin(ENGINE(), VM(), GC(), GAB_VAL_TO_BUILTIN(spec),
                       arity + 1, want);

          NEXT();
        } else {
          return vm_error(VM(), GAB_NOT_CALLABLE, "");
        }
      }

      gab_value spec = gab_obj_message_get(func, offset);

      STORE_FRAME();
      call_builtin(ENGINE(), VM(), GC(), GAB_VAL_TO_BUILTIN(spec), arity + 1,
                   want);

      NEXT();
    }
    }

    {

      u8 have;

      {
        u8 want;

        CASE_CODE(VARYIELD) : {
          have = *TOP() + READ_BYTE;
          want = READ_BYTE;

          goto complete_return;
        }

        CASE_CODE(YIELD) : {
          have = READ_BYTE;
          want = READ_BYTE;

          goto complete_yield;
        }

      complete_yield : {
        // Peek past the effect and increment value it captures
        for (u8 i = 0; i < have; i++) {
          gab_gc_iref(ENGINE(), VM(), GC(), PEEK_N(i + 1));
        }

        gab_value eff = GAB_VAL_OBJ(gab_obj_effect_create(
            CLOSURE(), IP(), have, want, TOP() - SLOTS(), SLOTS()));

        PUSH(eff);
        have++;

        // push a deref to the new effect
        gab_gc_dref(ENGINE(), VM(), GC(), eff);

        goto complete_return;
      }
      }

      CASE_CODE(VARRETURN) : {
        u8 addtl = READ_BYTE;
        have = *TOP() + addtl;
        goto complete_return;
      }

      // clang-format off
      CASE_CODE(RETURN_1):
      CASE_CODE(RETURN_2):
      CASE_CODE(RETURN_3):
      CASE_CODE(RETURN_4):
      CASE_CODE(RETURN_5):
      CASE_CODE(RETURN_6):
      CASE_CODE(RETURN_7):
      CASE_CODE(RETURN_8):
      CASE_CODE(RETURN_9):
      CASE_CODE(RETURN_10):
      CASE_CODE(RETURN_11):
      CASE_CODE(RETURN_12):
      CASE_CODE(RETURN_13):
      CASE_CODE(RETURN_14):
      CASE_CODE(RETURN_15):
      CASE_CODE(RETURN_16): {
        have = INSTR() - OP_RETURN_1 + 1;
        goto complete_return;
      }
      // clang-format on

    complete_return : {
      gab_value *from = TOP() - have;

      close_upvalue(ENGINE(), GC(), VM(), FRAME()->slots);

      TOP() = trim_return(VM(), from, FRAME()->slots, have, FRAME()->want);

      if (--FRAME() == VM()->call_stack) {
        // Increment and pop the module.
        gab_gc_iref(ENGINE(), VM(), GC(), PEEK());

        return POP();
      }

      LOAD_FRAME();
      NEXT();
    }
    }

    {
      gab_value key, index;
      u16 prop_offset;
      gab_obj_record *obj;

      CASE_CODE(LOAD_INDEX) : {
        key = PEEK();
        index = PEEK2();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);

        prop_offset = gab_obj_shape_find(obj->shape, key);

        DROP_N(2);
        goto complete_obj_get;
      }

      /*
       * We haven't seen any shapes yet.
       *
       * Ignore the cache and write to it.
       */
      CASE_CODE(LOAD_PROPERTY_ANA) : {
        key = READ_CONSTANT;

        index = PEEK();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);
        prop_offset = gab_obj_shape_find(obj->shape, key);

        // Transition state and store in the ache
        WRITE_INLINESHORT(prop_offset);
        WRITE_INLINEQWORD((u64)obj->shape);
        WRITE_BYTE(13, OP_LOAD_PROPERTY_MONO);

        DROP();
        goto complete_obj_get;
      }

      /*
       * We have seen more than one shape here.
       *
       * Ignore the cache and always do a raw load.
       *
       */
      CASE_CODE(LOAD_PROPERTY_POLY) : {
        key = READ_CONSTANT;
        SKIP_SHORT;
        READ_QWORD(gab_obj_shape);

        index = PEEK();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);

        prop_offset = gab_obj_shape_find(obj->shape, key);

        DROP();
        goto complete_obj_get;
      }

      /*
       * We have seen exactly one shape here.
       *
       * Use the cached type and offset.
       *
       * If we see a change, transition to a polymorphic load
       *
       */
      CASE_CODE(LOAD_PROPERTY_MONO) : {
        key = READ_CONSTANT;
        prop_offset = READ_SHORT;
        gab_obj_shape **cached_shape = READ_QWORD(gab_obj_shape *);

        index = PEEK();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);

        if (*cached_shape != obj->shape) {
          // Transition to a polymorphic load
          prop_offset = gab_obj_shape_find(obj->shape, key);
          WRITE_BYTE(13, OP_LOAD_PROPERTY_POLY);
        }

        DROP();
        goto complete_obj_get;
      }

    complete_obj_get : {
      PUSH(gab_obj_record_get(obj, prop_offset));
      NEXT();
    }
    }

    {

      gab_value value, key, index;
      gab_obj_record *obj;
      u16 prop_offset;

      CASE_CODE(STORE_INDEX) : {
        // Leave these on the stack for now in case we collect.
        value = PEEK();
        key = PEEK2();
        index = PEEK_N(3);

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);

        prop_offset = gab_obj_shape_find(obj->shape, key);

        if (prop_offset == UINT16_MAX) {
          // Update the obj and get the new offset.
          prop_offset = gab_obj_record_grow(ENGINE(), VM(), obj, key, value);
        } else {
          gab_gc_dref(ENGINE(), VM(), GC(),
                      gab_obj_record_get(obj, prop_offset));

          gab_obj_record_set(obj, prop_offset, value);
        }

        DROP_N(3);
        goto complete_obj_set;
      }

      /*
       * The shape has not stabilized. If it has here, write to the cache.
       */
      CASE_CODE(STORE_PROPERTY_ANA) : {
        key = READ_CONSTANT;

        value = PEEK();
        index = PEEK2();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);
        prop_offset = gab_obj_shape_find(obj->shape, key);

        if (prop_offset == UINT16_MAX) {
          // The shape here is not stable yet. The property didn't exist.
          prop_offset = gab_obj_record_grow(ENGINE(), VM(), obj, key, value);
          SKIP_SHORT;
          SKIP_QWORD;
        } else {
          // The shape is stable and the property existed.
          gab_gc_dref(ENGINE(), VM(), GC(),
                      gab_obj_record_get(obj, prop_offset));
          gab_obj_record_set(obj, prop_offset, value);

          // Write to the cache and transition to monomorphic
          WRITE_INLINESHORT(prop_offset);
          WRITE_INLINEQWORD((u64)obj->shape);

          WRITE_BYTE(13, OP_STORE_PROPERTY_MONO);
        }

        DROP_N(2);
        goto complete_obj_set;
      }

      /*
       * We tried to cache but the shape isn't stable. Revert to never caching.
       */
      CASE_CODE(STORE_PROPERTY_POLY) : {
        key = READ_CONSTANT;
        // Skip the cache
        SKIP_SHORT;
        SKIP_QWORD;

        value = PEEK();
        index = PEEK2();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);
        prop_offset = gab_obj_shape_find(obj->shape, key);

        if (prop_offset == UINT16_MAX) {
          // The shape here is not stable yet. The property didn't exist.
          prop_offset = gab_obj_record_grow(ENGINE(), VM(), obj, key, value);
        } else {
          // The shape is stable and the property existed.
          gab_gc_dref(ENGINE(), VM(), GC(),
                      gab_obj_record_get(obj, prop_offset));
          gab_obj_record_set(obj, prop_offset, value);
        }

        DROP_N(2);
        goto complete_obj_set;
      }

      /*
       * We have a stable shape in the cache.
       * If we miss here, transition to polymorphic
       */
      CASE_CODE(STORE_PROPERTY_MONO) : {
        key = READ_CONSTANT;
        // Use the cache
        prop_offset = READ_SHORT;
        gab_obj_shape **cached_shape = READ_QWORD(gab_obj_shape *);

        value = PEEK();
        index = PEEK2();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          return vm_error(VM(), GAB_NOT_RECORD, "Tried to index %.*s",
                          (i32)a->len, a->data);
        }

        obj = GAB_VAL_TO_RECORD(index);

        if (obj->shape == *cached_shape) {
          gab_gc_dref(ENGINE(), VM(), GC(),
                      gab_obj_record_get(obj, prop_offset));
          gab_obj_record_set(obj, prop_offset, value);
        } else {
          prop_offset = gab_obj_shape_find(obj->shape, key);

          if (prop_offset == UINT16_MAX) {
            prop_offset = gab_obj_record_grow(ENGINE(), VM(), obj, key, value);
          } else {
            gab_gc_dref(ENGINE(), VM(), GC(),
                        gab_obj_record_get(obj, prop_offset));
            gab_obj_record_set(obj, prop_offset, value);
          }

          // Transition to polymorphic
          WRITE_BYTE(13, OP_STORE_PROPERTY_POLY);
        }

        DROP_N(2);
        goto complete_obj_set;
      }

    complete_obj_set : {
      gab_gc_iref(ENGINE(), VM(), GC(), value);

      PUSH(value);

      NEXT();
    }
    }

    CASE_CODE(NOP) : { NEXT(); }

    CASE_CODE(CONSTANT) : {
      PUSH(READ_CONSTANT);
      NEXT();
    }

    CASE_CODE(NEGATE) : {
      if (!GAB_VAL_IS_NUMBER(PEEK())) {
        STORE_FRAME();
        gab_obj_string *a =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK()));
        return vm_error(VM(), GAB_NOT_NUMERIC, "Tried to negate %.*s",
                        (i32)a->len, a->data);
      }
      gab_value num = GAB_VAL_NUMBER(-GAB_VAL_TO_NUMBER(POP()));
      PUSH(num);
      NEXT();
    }

    CASE_CODE(SUBTRACT) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, f64, -);
      NEXT();
    }

    CASE_CODE(ADD) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, f64, +);
      NEXT();
    }

    CASE_CODE(DIVIDE) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, f64, /);
      NEXT();
    }

    CASE_CODE(MODULO) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, u64, %);
      NEXT();
    }

    CASE_CODE(MULTIPLY) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, f64, *);
      NEXT();
    }

    CASE_CODE(GREATER) : {
      BINARY_OPERATION(GAB_VAL_BOOLEAN, f64, >);
      NEXT();
    }

    CASE_CODE(GREATER_EQUAL) : {
      BINARY_OPERATION(GAB_VAL_BOOLEAN, f64, >=);
      NEXT();
    }

    CASE_CODE(LESSER) : {
      BINARY_OPERATION(GAB_VAL_BOOLEAN, f64, <);
      NEXT();
    }

    CASE_CODE(LESSER_EQUAL) : {
      BINARY_OPERATION(GAB_VAL_BOOLEAN, f64, <=);
      NEXT();
    }

    CASE_CODE(BITWISE_AND) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, u64, &);
      NEXT();
    }

    CASE_CODE(BITWISE_OR) : {
      BINARY_OPERATION(GAB_VAL_NUMBER, u64, |);
      NEXT();
    }

    CASE_CODE(EQUAL) : {
      gab_value a = POP();
      gab_value b = POP();
      PUSH(GAB_VAL_BOOLEAN(a == b));
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

    CASE_CODE(CONCAT) : {
      if (!GAB_VAL_IS_STRING(PEEK()) + !GAB_VAL_IS_STRING(PEEK2())) {
        STORE_FRAME();
        gab_obj_string *a =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK()));

        gab_obj_string *b =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK2()));

        return vm_error(VM(), GAB_NOT_STRING,
                        "Tried to concatenate %.*s and %.*s", (i32)b->len,
                        b->data, (i32)a->len, a->data);
      }

      gab_obj_string *b = GAB_VAL_TO_STRING(POP());
      gab_obj_string *a = GAB_VAL_TO_STRING(POP());

      gab_obj_string *obj = gab_obj_string_concat(ENGINE(), a, b);

      PUSH(GAB_VAL_OBJ(obj));

      NEXT();
    }

    CASE_CODE(STRINGIFY) : {
      if (!GAB_VAL_IS_STRING(PEEK())) {
        gab_value str = gab_val_to_string(ENGINE(), POP());

        PUSH(str);
      }
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

    CASE_CODE(NOT) : {
      gab_value val = GAB_VAL_BOOLEAN(gab_val_falsey(POP()));
      PUSH(val);
      NEXT();
    }

    CASE_CODE(ASSERT) : {
      if (GAB_VAL_IS_NIL(PEEK())) {
        STORE_FRAME();
        return vm_error(VM(), GAB_ASSERTION_FAILED, "");
      }
      NEXT();
    }

    CASE_CODE(TYPE) : {
      PEEK() = gab_typeof(ENGINE(), PEEK());
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

    CASE_CODE(SPREAD) : {
      u8 want = READ_BYTE;

      gab_value index = POP();

      if (GAB_VAL_IS_RECORD(index)) {
        gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

        u8 have = obj->is_dynamic ? obj->dynamic_values.len : obj->static_len;

        TOP() = trim_return(VM(),
                            obj->is_dynamic ? obj->dynamic_values.data
                                            : obj->static_values,
                            TOP(), have, want);
      } else if (GAB_VAL_IS_SHAPE(index)) {
        gab_obj_shape *shape = GAB_VAL_TO_SHAPE(index);

        u8 have = shape->properties.len;

        TOP() = trim_return(VM(), shape->keys, TOP(), have, want);
      } else {
        STORE_FRAME();
        gab_obj_string *a =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
        return vm_error(VM(), GAB_NOT_RECORD, "Tried to spread %.*s",
                        (i32)a->len, a->data);
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
      PUSH(UPVALUE(INSTR()- OP_LOAD_UPVALUE_0));
      NEXT();
    }

    CASE_CODE(STORE_UPVALUE_0) :
    CASE_CODE(STORE_UPVALUE_1) :
    CASE_CODE(STORE_UPVALUE_2) :
    CASE_CODE(STORE_UPVALUE_3) :
    CASE_CODE(STORE_UPVALUE_4) :
    CASE_CODE(STORE_UPVALUE_5) :
    CASE_CODE(STORE_UPVALUE_6) :
    CASE_CODE(STORE_UPVALUE_7) :
    CASE_CODE(STORE_UPVALUE_8) : {
      UPVALUE(INSTR()- OP_STORE_UPVALUE_0) = PEEK();
      NEXT();
    }

    CASE_CODE(LOAD_CONST_UPVALUE_0) :
    CASE_CODE(LOAD_CONST_UPVALUE_1) :
    CASE_CODE(LOAD_CONST_UPVALUE_2) :
    CASE_CODE(LOAD_CONST_UPVALUE_3) :
    CASE_CODE(LOAD_CONST_UPVALUE_4) :
    CASE_CODE(LOAD_CONST_UPVALUE_5) :
    CASE_CODE(LOAD_CONST_UPVALUE_6) :
    CASE_CODE(LOAD_CONST_UPVALUE_7) :
    CASE_CODE(LOAD_CONST_UPVALUE_8) : {
      PUSH(CONST_UPVALUE(INSTR()- OP_LOAD_CONST_UPVALUE_0));
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

    CASE_CODE(LOAD_CONST_UPVALUE) : {
      PUSH(CONST_UPVALUE(READ_BYTE));
      NEXT();
    }

    CASE_CODE(STORE_UPVALUE) : {
      UPVALUE(READ_BYTE) = PEEK();
      NEXT();
    }

    CASE_CODE(POP_STORE_UPVALUE) : {
      UPVALUE(READ_BYTE) = POP();
      NEXT();
    }

    CASE_CODE(CLOSE_UPVALUE) : {
      close_upvalue(ENGINE(), GC(), VM(), SLOTS() + READ_BYTE);
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

    CASE_CODE(CLOSURE) : {
      gab_obj_prototype *p = READ_PROTOTYPE;

      gab_value *upvalues = TOP();

      for (int i = 0; i < p->nupvalues; i++) {
        u8 flags = READ_BYTE;
        u8 index = READ_BYTE;

        if (flags & FLAG_LOCAL) {
          if (flags & FLAG_MUTABLE) {
            PUSH(capture_upvalue(ENGINE(), VM(), GC(), FRAME()->slots + index));
          } else {
            PUSH(LOCAL(index));
            gab_gc_iref(ENGINE(), VM(), GC(), upvalues[i]);
          }
        } else {
          PUSH(CLOSURE()->upvalues[index]);
          gab_gc_iref(ENGINE(), VM(), GC(), upvalues[i]);
        }
      }

      DROP_N(p->nupvalues);

      gab_value obj = GAB_VAL_OBJ(gab_obj_closure_create(p, upvalues));

      gab_gc_dref(ENGINE(), VM(), GC(), obj);

      PUSH(obj);

      NEXT();
    }

    CASE_CODE(MESSAGE) : {
      gab_obj_prototype *p = READ_PROTOTYPE;
      gab_obj_message *m = READ_MESSAGE;
      gab_value r = PEEK();

      if (gab_obj_message_find(m, r) != UINT16_MAX) {
        STORE_FRAME();
        gab_obj_string *s = GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), r));
        return vm_error(VM(), GAB_NOT_CALLABLE,
                        "Specialization already exists for %.*s on %.*s",
                        (i32)s->len, s->data, (i32)m->name.len, m->name.data);
      }

      // Artificially use the stack to store the upvalues
      // as we're finding them to capture.
      // This is done so that the GC knows they're there.
      gab_value *upvalues = TOP();

      for (int i = 0; i < p->nupvalues; i++) {
        u8 flags = READ_BYTE;
        u8 index = READ_BYTE;

        if (flags & FLAG_LOCAL) {
          if (flags & FLAG_MUTABLE) {
            PUSH(capture_upvalue(ENGINE(), VM(), GC(), FRAME()->slots + index));
          } else {
            PUSH(LOCAL(index));
            gab_gc_iref(ENGINE(), VM(), GC(), upvalues[i]);
          }
        } else {
          PUSH(CLOSURE()->upvalues[index]);
          gab_gc_iref(ENGINE(), VM(), GC(), upvalues[i]);
        }
      }

      DROP_N(p->nupvalues + 1);

      gab_value obj = GAB_VAL_OBJ(gab_obj_closure_create(p, upvalues));

      gab_gc_iref(ENGINE(), VM(), GC(), r);

      gab_obj_message_insert(m, r, obj);

      PUSH(GAB_VAL_OBJ(m));

      NEXT();
    }

    {
      gab_obj_shape *shape;
      u8 size;

      CASE_CODE(OBJECT_RECORD_DEF) : {
        gab_obj_string *name = READ_STRING;

        size = READ_BYTE;

        shape = gab_obj_shape_create(ENGINE(), VM(), size, 2, TOP() - size * 2);

        shape->name = gab_obj_string_ref(name);

        goto complete_record;
      }

      CASE_CODE(OBJECT_RECORD) : {
        size = READ_BYTE;

        shape = gab_obj_shape_create(ENGINE(), VM(), size, 2, TOP() - size * 2);

        goto complete_record;
      }

    complete_record : {
      // Increment the rc of the values only
      for (u64 i = 1; i < size * 2; i += 2) {
        gab_gc_iref(ENGINE(), VM(), GC(), PEEK_N(i));
      }

      gab_value obj = GAB_VAL_OBJ(
          gab_obj_record_create(shape, size, 2, TOP() + 1 - (size * 2)));

      DROP_N(size * 2);

      PUSH(obj);

      gab_gc_dref(ENGINE(), VM(), GC(), obj);

      NEXT();
    }
    }

    CASE_CODE(OBJECT_ARRAY) : {
      u8 size = READ_BYTE;

      for (u64 i = 1; i < size; i++) {
        gab_gc_iref(ENGINE(), VM(), GC(), PEEK_N(i));
      }

      gab_obj_shape *shape = gab_obj_shape_create_array(ENGINE(), VM(), size);

      gab_value obj =
          GAB_VAL_OBJ(gab_obj_record_create(shape, size, 1, TOP() - (size)));

      DROP_N(size);

      PUSH(obj);

      gab_gc_dref(ENGINE(), VM(), GC(), obj);

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
#undef BINARY_OPERATION
