#include "include/char.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/object.h"

#include <stdarg.h>
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

void dump_vm_error(gab_engine *gab, gab_vm *vm, gab_module *mod, gab_status e,
                   const char *help_fmt, ...) {
  if (!(gab->flags & GAB_FLAG_DUMP_ERROR)) {
    return;
  }

  gab_call_frame *frame = vm->call_stack + 1;

  while (frame <= vm->frame) {
    s_i8 func_name = frame->closure->func->name;

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

    if (frame == vm->frame) {
      fprintf(stderr,
              "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET
              "] Error near " ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET
              ":\n\t\u256d " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%.*s"
              "\n\t\u2502"
              "\n\t\u2570\u2500> ",
              (i32)func_name.len, func_name.data, tok, curr_row, curr_src_len,
              curr_src_start);

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
              "\n\t"
              "\n",
              (i32)func_name.len, func_name.data, curr_row, curr_src_len,
              curr_src_start);
    }

    frame++;
  }
}

void gab_vm_create(gab_vm *self) {
  self->open_upvalues = NULL;
  memset(self->stack, 0, sizeof(self->stack));

  self->frame = self->call_stack;
  self->top = self->stack;

  gab_gc_create(&self->gc);
}

void gab_vm_destroy(gab_vm* vm) {
    gab_gc_collect(&vm->gc, vm);
    gab_gc_destroy(&vm->gc);
};

void dump_frame(gab_vm *vm, gab_value *top, const char *name) {
  printf("Frame at %s:\n-----------------\n", name);
  gab_value *tracker = top - 1;
  while (tracker >= vm->frame->slots) {
    printf("|%lu|", tracker - vm->frame->slots);
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
    *to++ = GAB_VAL_NULL();

  *to = got;
  return to;
}

static inline gab_value capture_upvalue(gab_vm *vm, gab_gc *gc,
                                        gab_value *local) {
  gab_obj_upvalue *prev_upvalue = NULL;
  gab_obj_upvalue *upvalue = vm->open_upvalues;

  while (upvalue != NULL && upvalue->data > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->data == local) {
    // Capture this upvalue with a new referemce
    gab_gc_iref(gc, vm, GAB_VAL_OBJ(upvalue));
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

static inline void close_upvalue(gab_gc *gc, gab_vm *vm, gab_value *local) {
  while (vm->open_upvalues != NULL && vm->open_upvalues->data >= local) {
    gab_obj_upvalue *upvalue = vm->open_upvalues;

    upvalue->closed = *upvalue->data;
    gab_gc_iref(gc, vm, upvalue->closed);

    upvalue->data = &upvalue->closed;

    vm->open_upvalues = upvalue->next;
  }
}

static inline gab_value failure(gab_vm *vm) {
  vm->top = vm->stack;
  vm->frame = vm->call_stack;
  return GAB_VAL_NULL();
}

gab_value gab_vm_run(gab_engine *gab, i32 vm, gab_module *mod, u8 argc,
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
     We do a plus because it is slightly faster than an or.
     It is faster because there is no reliable branch to predict,
     and short circuiting can't help us.
  */
#define BINARY_OPERATION(value_type, operation_type, operation)                \
  if ((GAB_VAL_IS_NUMBER(PEEK()) + GAB_VAL_IS_NUMBER(PEEK2())) < 2) {          \
    STORE_FRAME();                                                             \
    gab_obj_string *a =                                                        \
        GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK()));                \
                                                                               \
    gab_obj_string *b =                                                        \
        GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), PEEK2()));               \
                                                                               \
    dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_NUMERIC,                   \
                  "Tried to" #operation " %.*s and %.*s", (i32)b->len,         \
                  b->data, (i32)a->len, a->data);                              \
    return failure(VM());                                                      \
  }                                                                            \
  operation_type b = GAB_VAL_TO_NUMBER(POP());                                 \
  operation_type a = GAB_VAL_TO_NUMBER(POP());                                 \
  PUSH(value_type(a operation b));

/*
  Lots of helper macros.
*/
#define VM() (vm_ptr)
#define ENGINE() (gab)
#define GC() (&VM()->gc)
#define INSTR() (instr)
#define FRAME() (frame)
#define CLOSURE() (FRAME()->closure)
#define MODULE() (mod)
#define IP() (ip)
#define TOP() (VM()->top)
#define SLOTS() (slots)
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
#define PEEK_N(n) (*(TOP() - n))

#define WRITE_SHORT(n) ((IP()[-2] = ((n) >> 8) & 0xff), IP()[-1] = (n)&0xff)

#define READ_INLINECACHE(type) (IP() += 8, (type **)(IP() - 8))
#define READ_BYTE (*IP()++)
#define READ_SHORT (IP() += 2, (((u16)IP()[-2] << 8) | IP()[-1]))
#define READ_CONSTANT (MOD_CONSTANT(READ_SHORT))
#define READ_STRING (GAB_VAL_TO_STRING(READ_CONSTANT))
#define READ_FUNCTION (GAB_VAL_TO_FUNCTION(READ_CONSTANT))

#define STORE_FRAME()                                                          \
  ({                                                                           \
    FRAME()->ip = IP();                                                        \
    VM()->frame = FRAME();                                                     \
  })

#define LOAD_FRAME()                                                           \
  ({                                                                           \
    IP() = FRAME()->ip;                                                        \
    SLOTS() = FRAME()->slots;                                                  \
  })

  /*
   ----------- BEGIN RUN BODY -----------
  */

  gab_vm *vm_ptr = v_gab_vm_ref_at(&gab->vms, vm);
  register u8 instr;
  register u8 *ip;
  register gab_call_frame *frame = VM()->frame;
  register gab_value *slots = TOP();

  // Setup for call to main function
  gab_value main_closure = MOD_CONSTANT(MODULE()->main);

  PUSH(main_closure);
  for (u8 i = 0; i < argc; i++) {
    PUSH(argv[i]);
  }

  LOOP() {
    {
      u8 arity, want;
      gab_value callee;

      // Setup the call to main function
      arity = argc;
      want = 1;
      callee = PEEK_N(argc - 1);
      goto complete_call;

      CASE_CODE(VARCALL) : {
        u8 addtl = READ_BYTE;

        want = READ_BYTE;
        arity = *TOP() + addtl;
        callee = PEEK_N(arity - 1);
        goto complete_call;
      }

      // clang-format off
      CASE_CODE(CALL_0):
      CASE_CODE(CALL_1):
      CASE_CODE(CALL_2):
      CASE_CODE(CALL_3):
      CASE_CODE(CALL_4):
      CASE_CODE(CALL_5):
      CASE_CODE(CALL_6):
      CASE_CODE(CALL_7):
      CASE_CODE(CALL_8):
      CASE_CODE(CALL_9):
      CASE_CODE(CALL_10):
      CASE_CODE(CALL_11):
      CASE_CODE(CALL_12):
      CASE_CODE(CALL_13):
      CASE_CODE(CALL_14):
      CASE_CODE(CALL_15):
      CASE_CODE(CALL_16): {
        want = READ_BYTE;

        arity = INSTR()- OP_CALL_0;
        callee = PEEK_N(arity - 1);
        goto complete_call;
      }
      // clang-format on

    complete_call : {

      if (GAB_VAL_IS_CLOSURE(callee)) {
        STORE_FRAME();

        gab_obj_closure *closure = GAB_VAL_TO_CLOSURE(callee);

        while (arity < closure->func->narguments)
          PUSH(GAB_VAL_NULL()), arity++;

        while (arity > closure->func->narguments)
          DROP(), arity--;

        FRAME()++;
        FRAME()->closure = closure;
        FRAME()->ip = MODULE()->bytecode.data + closure->func->offset;
        FRAME()->slots = TOP() - arity - 1;
        FRAME()->expected_results = want;

        TOP() += CLOSURE()->func->nlocals;

        LOAD_FRAME();
      } else if (GAB_VAL_IS_BUILTIN(callee)) {
        gab_obj_builtin *builtin = GAB_VAL_TO_BUILTIN(callee);

        gab_value result =
            (*builtin->function)(ENGINE(), vm, arity, TOP() - arity);

        TOP() -= arity + 1;

        TOP() = trim_return(VM(), &result, TOP(), 1, want);

        gab_gc_dref(GC(), VM(), result);
      } else {
        STORE_FRAME();
        gab_obj_string *a =
            GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), callee));
        dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_FUNCTION,
                      "Tried to call %.*s", (i32)a->len, a->data);
        return failure(VM());
      }

      NEXT();
    }
    }

    {

      u8 have, addtl;

      CASE_CODE(VARRETURN) : {
        addtl = READ_BYTE;
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

      close_upvalue(GC(), VM(), FRAME()->slots);

      TOP() = trim_return(VM(), from, FRAME()->slots, have,
                          FRAME()->expected_results);

      if (--FRAME() == VM()->call_stack) {
        FRAME()++;

        // Increment and pop the module.
        gab_gc_iref(GC(), VM(), PEEK());

        return POP();
      }

      LOAD_FRAME();
      NEXT();
    }
    }

    {
      gab_value prop, index;
      i16 prop_offset;
      gab_obj_record *obj;

      CASE_CODE(GET_INDEX) : {
        prop = PEEK();
        index = PEEK2();

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_RECORD,
                        "Tried to index %.*s", (i32)a->len, a->data);
          return failure(VM());
        }

        obj = GAB_VAL_TO_RECORD(index);

        prop_offset = gab_obj_shape_find(obj->shape, prop);

        DROP_N(2);
        goto complete_obj_get;
      }

      CASE_CODE(GET_PROPERTY) : {
        prop = READ_CONSTANT;
        index = PEEK();

        gab_obj_shape **cached_shape = READ_INLINECACHE(gab_obj_shape);
        prop_offset = READ_SHORT;

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_RECORD,
                        "Tried to index %.*s", (i32)a->len, a->data);
          return failure(VM());
        }

        obj = GAB_VAL_TO_RECORD(index);

        if (*cached_shape != obj->shape) {
          // The cache hasn't been created yet.
          *cached_shape = obj->shape;
          prop_offset = gab_obj_shape_find(obj->shape, prop);
          // Writes into the short just before the ip.
          WRITE_SHORT(prop_offset);
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

      gab_value value, prop, index;
      gab_obj_record *obj;
      i16 prop_offset;
      boolean was_interned;

      CASE_CODE(SET_INDEX) : {
        // Leave these on the stack for now in case we collect.
        value = PEEK();
        prop = PEEK2();
        index = PEEK_N(3);

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_RECORD,
                        "Tried to index %.*s", (i32)a->len, a->data);
          return failure(VM());
        }

        obj = GAB_VAL_TO_RECORD(index);

        prop_offset = gab_obj_shape_find(obj->shape, prop);

        if (prop_offset < 0) {
          // The key didn't exist on the old shape.
          // Create a new shape and update the cache.
          gab_obj_shape *shape =
              gab_obj_shape_extend(ENGINE(), obj->shape, &was_interned, prop);

          if (!was_interned)
            gab_gc_iref(GC(), VM(), prop);

          // Update the obj and get the new offset.
          prop_offset = gab_obj_record_extend(obj, shape, value);
        }

        DROP_N(3);
        goto complete_obj_set;
      }

      CASE_CODE(SET_PROPERTY) : {
        prop = READ_CONSTANT;
        value = PEEK();
        index = PEEK2();

        gab_obj_shape **cached_shape = READ_INLINECACHE(gab_obj_shape);

        prop_offset = READ_SHORT;

        if (!GAB_VAL_IS_RECORD(index)) {
          STORE_FRAME();
          gab_obj_string *a =
              GAB_VAL_TO_STRING(gab_val_to_string(ENGINE(), index));
          dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_RECORD,
                        "Tried to index %.*s", (i32)a->len, a->data);
          return failure(VM());
        }

        obj = GAB_VAL_TO_RECORD(index);

        if (*cached_shape != obj->shape) {
          // The cache hasn't been created yet.
          *cached_shape = obj->shape;

          prop_offset = gab_obj_shape_find(obj->shape, prop);
          // Writes into the short just before the ip.
          WRITE_SHORT(prop_offset);
        }

        if (prop_offset < 0) {
          // The key didn't exist on the old shape.
          // Create a new shape and update the cache.
          *cached_shape = gab_obj_shape_extend(ENGINE(), *cached_shape,
                                               &was_interned, prop);
          if (!was_interned)
            gab_gc_iref(GC(), VM(), prop);

          // Update the obj and get the new offset.
          prop_offset = gab_obj_record_extend(obj, *cached_shape, value);

          // Write the offset.
          WRITE_SHORT(prop_offset);
        }

        DROP_N(2);
        goto complete_obj_set;
      }

    complete_obj_set : {

      gab_gc_iref(GC(), VM(), value);
      gab_gc_dref(GC(), VM(), gab_obj_record_get(obj, prop_offset));

      gab_obj_record_set(obj, prop_offset, value);

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
        dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_NUMERIC,
                      "Tried to negate %.*s", (i32)a->len, a->data);
        return failure(VM());
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

        dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_STRING,
                      "Tried to concatenate %.*s and %.*s", (i32)b->len,
                      b->data, (i32)a->len, a->data);
        return failure(VM());
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

    CASE_CODE(PUSH_NULL) : {
      PUSH(GAB_VAL_NULL());
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
      if (GAB_VAL_IS_NULL(PEEK())) {
        STORE_FRAME();
        dump_vm_error(ENGINE(), VM(), MODULE(), GAB_ASSERTION_FAILED, "");
        return failure(VM());
      }
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

    CASE_CODE(SPREAD) : {
      u8 want = READ_BYTE;

      gab_value index = POP();

      if (GAB_VAL_IS_RECORD(index)) {
        gab_obj_record *obj = GAB_VAL_TO_RECORD(index);

        u8 have = obj->is_dynamic ? obj->dynamic_values.len : obj->static_size;

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
        dump_vm_error(ENGINE(), VM(), MODULE(), GAB_NOT_RECORD,
                      "Tried to spread %.*s", (i32)a->len, a->data);
        return failure(VM());
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
      close_upvalue(GC(), VM(), SLOTS() + READ_BYTE);
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
      gab_obj_function *func = READ_FUNCTION;

      gab_value *upvalues = TOP();

      // Artificially use the stack to store the upvalues
      // as we're finding them to capture.
      // This is done so that the GC knows they're there.

      for (int i = 0; i < func->nupvalues; i++) {
        u8 flags = READ_BYTE;
        u8 index = READ_BYTE;

        if (flags & FLAG_LOCAL) {
          if (flags & FLAG_MUTABLE) {
            PUSH(capture_upvalue(VM(), GC(), FRAME()->slots + index));
          } else {
            PUSH(LOCAL(index));
            gab_gc_iref(GC(), VM(), upvalues[i]);
          }
        } else {
          PUSH(CLOSURE()->upvalues[index]);
          gab_gc_iref(GC(), VM(), upvalues[i]);
        }
      }

      DROP_N(func->nupvalues);

      gab_value obj = GAB_VAL_OBJ(gab_obj_closure_create(func, upvalues));

      PUSH(obj);

      gab_gc_dref(GC(), VM(), obj);

      NEXT();
    }

    {
      gab_obj_shape *shape;
      u8 size;
      boolean was_interned;

      CASE_CODE(OBJECT_RECORD_DEF) : {
        gab_obj_string *name = READ_STRING;

        size = READ_BYTE;

        shape = gab_obj_shape_create(ENGINE(), &was_interned, size, 2,
                                     TOP() - size * 2);

        shape->name = gab_obj_string_ref(name);

        if (was_interned) {
          // Increment the rc of the values only
          for (u64 i = 1; i < size * 2; i += 2) {
            gab_gc_iref(GC(), VM(), PEEK_N(i));
          }
        } else {
          // Increment the rc for values and keys.
          for (u64 i = 1; i < size * 2; i++) {
            gab_gc_iref(GC(), VM(), PEEK_N(i));
          }
        }

        goto complete_record;
      }

      CASE_CODE(OBJECT_RECORD) : {
        size = READ_BYTE;

        shape = gab_obj_shape_create(ENGINE(), &was_interned, size, 2,
                                     TOP() - size * 2);

        if (was_interned) {
          // Increment the rc of the values only
          for (u64 i = 1; i < size * 2; i += 2) {
            gab_gc_iref(GC(), VM(), PEEK_N(i));
          }
        } else {
          // Increment the rc for values and keys.
          for (u64 i = 1; i < size * 2; i++) {
            gab_gc_iref(GC(), VM(), PEEK_N(i));
          }
        }

        goto complete_record;
      }

    complete_record : {
      gab_value obj = GAB_VAL_OBJ(
          gab_obj_record_create(shape, TOP() + 1 - (size * 2), size, 2));

      DROP_N(size * 2);

      PUSH(obj);

      gab_gc_dref(GC(), VM(), obj);

      NEXT();
    }
    }

    CASE_CODE(OBJECT_ARRAY) : {
      u8 size = READ_BYTE;

      for (u64 i = 1; i < size; i++) {
        gab_gc_iref(GC(), VM(), PEEK_N(i));
      }

      gab_obj_shape *shape = gab_obj_shape_create_array(ENGINE(), size);

      gab_value obj =
          GAB_VAL_OBJ(gab_obj_record_create(shape, TOP() - (size), size, 1));

      DROP_N(size);

      PUSH(obj);

      gab_gc_dref(GC(), VM(), obj);

      NEXT();
    }
  }
  // This should be unreachable
  return GAB_VAL_NULL();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OPERATION
