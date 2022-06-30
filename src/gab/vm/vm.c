#include "vm.h"
#include "gc.h"
#include <stdio.h>
#include <sys/types.h>

void gab_vm_create(gab_vm *self) {
  self->open_upvalues = NULL;
  self->stack_top = self->stack;
  self->frame = self->call_stack;
}

void dump_frame(gab_vm *vm, const char *name) {
  printf("Frame at %s:\n-----------------\n", name);
  gab_value *tracker = vm->stack_top - 1;
  while (tracker >= vm->frame->slots) {
    printf("|%lu|", tracker - vm->frame->slots);
    gab_val_dump(*tracker--);
    printf("\n");
  }
  printf("---------------\n");
};

void dump_stack(gab_vm *vm, const char *name) {
  printf("Stack at %s:\n-----------------\n", name);
  gab_value *tracker = vm->stack_top - 1;
  while (tracker >= vm->stack) {
    printf("|");
    gab_val_dump(*tracker--);
    printf("\n");
  }
  printf("---------------\n");
}

static inline void trim_return(gab_vm *vm, gab_value *from, u8 have, u8 want) {
  u8 nulls = have == 0;

  if ((have != want) && (want != VAR_RET)) {
    if (have > want)
      have = want;
    else
      nulls = want - have;
  }

  const u8 got = have + nulls;

  while (have--)
    *vm->stack_top++ = *from++;

  while (nulls--)
    *vm->stack_top++ = GAB_VAL_NULL();

  *vm->stack_top = got;
}

static inline gab_obj_upvalue *capture_upvalue(gab_engine *eng, gab_vm *vm,
                                               gab_value *local) {
  gab_obj_upvalue *prev_upvalue = NULL;
  gab_obj_upvalue *upvalue = vm->open_upvalues;

  while (upvalue != NULL && upvalue->data > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->data == local) {
    return upvalue;
  }

  // The captured value's reference count has to be incremented
  // Before the allocation which creates the new upvalue.
  // This occurs in 'capture_upvalue'
  gab_gc_push_inc_if_obj_ref(eng->gc, *local);

  gab_obj_upvalue *new_upvalue = gab_obj_upvalue_create(eng, local);

  gab_gc_push_dec_obj_ref(eng->gc, (gab_obj *)new_upvalue);

  new_upvalue->next = upvalue;
  if (prev_upvalue == NULL) {
    vm->open_upvalues = new_upvalue;
  } else {
    prev_upvalue->next = new_upvalue;
  }
  return new_upvalue;
}

static inline void close_upvalue(gab_vm *vm, gab_value *local) {
  while (vm->open_upvalues != NULL && vm->open_upvalues->data >= local) {
    gab_obj_upvalue *upvalue = vm->open_upvalues;
    upvalue->closed = *upvalue->data;
    upvalue->data = &upvalue->closed;
    vm->open_upvalues = upvalue->next;
  }
}

gab_result *gab_engine_run(gab_engine *eng, gab_vm *vm, gab_obj_closure *main) {

#if GAB_LOG_EXECUTION
  /*
    If we're logging execution, create the instruction name table and the
    log macro.
  */
  static const char *instr_name[] = {
#define OP_CODE(name) #name,
#include "../compiler/bytecode.h"
#undef OP_CODE
  };

#define LOG() printf("OP_%s\n", instr_name[*(ip + 1)])

#else
#define LOG()
#endif

#if GAB_GOTO
  /*
    If we're using computed gotos, create the jump table and the dispatch
    instructions.
  */
  static void *dispatch_table[] = {
#define OP_CODE(name) &&code_##name,
#include "../compiler/bytecode.h"
#undef OP_CODE
  };

#define CASE_CODE(name) code_##name
#define LOOP()
#define NEXT()                                                                 \
  do {                                                                         \
    LOG();                                                                     \
    goto *dispatch_table[instr = READ_BYTE];                                   \
  } while (0)

#else
  /*
    Without computed gotos, we just use a hugt switch statement.
  */

#define CASE_CODE(name) case OP_##name
#define NEXT()                                                                 \
  LOG();                                                                       \
  goto loop;

#define LOOP()                                                                 \
  loop:                                                                        \
  switch (instr = READ_BYTE)

#endif

  /*
     We do a plus because it is slightly faster than an or.
     It is faster because there is no reliable branch to predict,
     and short circuiting can't help us.
  */
#define BINARY_OPERATION(value_type, operation_type, operation)                \
  if ((GAB_VAL_IS_NUMBER(PEEK()) + GAB_VAL_IS_NUMBER(PEEK2())) < 2) {          \
    STORE_FRAME();                                                             \
    return gab_run_fail(VM(), "Binary operations only work on numbers");       \
  }                                                                            \
  operation_type b = GAB_VAL_TO_NUMBER(POP());                                 \
  operation_type a = GAB_VAL_TO_NUMBER(POP());                                 \
  PUSH(value_type(a operation b));

/*
  Lots of helper macros.
*/
#define VM() (vm)
#define ENGINE() (eng)
#define GC() (ENGINE()->gc)
#define LOCAL(i) (VM()->frame->slots[i])
#define UPVALUE(i) (VM()->frame->closure->upvalues[i])
#define INSTR() (instr)

#define PUSH(value) (*VM()->stack_top++ = value)
#define POP() (*(--VM()->stack_top))
#define DROP() (VM()->stack_top--)
#define POP_N(n) (VM()->stack_top -= n)
#define DROP_N(n) (VM()->stack_top -= n)
#define PEEK() (*(VM()->stack_top - 1))
#define PEEK2() (*(VM()->stack_top - 2))
#define PEEK_N(n) (*(VM()->stack_top - n))

#define READ_BYTE (*ip++)
#define READ_SHORT (ip += 2, (((u16)ip[-2] << 8) | ip[-1]))
#define WRITE_SHORT(n) ((ip[-2] = ((n) >> 8) & 0xff), ip[-1] = (n)&0xff)
#define READ_INLINECACHE(type) (ip += 8, (type **)(ip - 8))

#define READ_CONSTANT (ENGINE()->constants.keys.data[READ_SHORT])
#define READ_STRING (GAB_VAL_TO_STRING(READ_CONSTANT))

#define STORE_FRAME() (VM()->frame->ip = ip)
#define LOAD_FRAME() (ip = VM()->frame->ip)

  /*
   ----------- BEGIN RUN BODY -----------
  */

  gab_vm_create(VM());
  ENGINE()->vm = VM();

  // Turn on garbage collection.
  ENGINE()->gc->active = true;

  PUSH(GAB_VAL_OBJ(main));
  PUSH(ENGINE()->std);

  gab_gc_push_dec_obj_ref(GC(), (gab_obj *)main);

  register u8 *ip;
  register u8 instr;

  LOOP() {
    {
      u8 arity, want;
      gab_value callee;

      arity = 1;
      want = 1;
      callee = PEEK2();
      goto complete_call;

      CASE_CODE(VARCALL) : {
        u8 addtl = READ_BYTE;

        want = READ_BYTE;
        arity = *VM()->stack_top + addtl;
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

        arity = INSTR() - OP_CALL_0;
        callee = PEEK_N(arity - 1);

        goto complete_call;
      }
      // clang-format on

    complete_call : {
      STORE_FRAME();

      if (!GAB_VAL_IS_OBJ(callee)) {
        return gab_run_fail(VM(), "Expected a callable");
      }

      if (GAB_VAL_TO_OBJ(callee)->kind == OBJECT_CLOSURE) {
        gab_obj_closure *closure = GAB_VAL_TO_CLOSURE(callee);

        // Combine the two error branches into a single case.
        // Check for stack overflow here
        if (arity != closure->func->narguments) {
          return gab_run_fail(VM(), "Wrong number of arguments");
        }

        VM()->frame++;
        VM()->frame->closure = closure;
        VM()->frame->ip =
            closure->func->module->bytecode.data + closure->func->offset;
        VM()->frame->slots = VM()->stack_top - arity - 1;
        VM()->frame->expected_results = want;

        VM()->stack_top += closure->func->nlocals;

        LOAD_FRAME();
      } else if (GAB_VAL_TO_OBJ(callee)->kind == OBJECT_BUILTIN) {
        gab_obj_builtin *builtin = GAB_VAL_TO_BUILTIN(callee);

        if (arity != builtin->narguments && builtin->narguments != VAR_RET) {
          return gab_run_fail(VM(), "Wrong number of arguments");
        }

        gab_value result =
            (*builtin->function)(ENGINE(), VM()->stack_top - arity, arity);

        VM()->stack_top -= arity + 1;
        u8 have = 1;

        trim_return(VM(), &result, have, want);

      } else {
        return gab_run_fail(VM(), "Expected a callable");
      }

      NEXT();
    }
    }

    {

      u8 have, addtl;

      CASE_CODE(VARRETURN) : {
        addtl = READ_BYTE;
        have = *VM()->stack_top + addtl;

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
        have = INSTR()- OP_RETURN_1 + 1;

        goto complete_return;
      }
      // clang-format on

    complete_return : {

      gab_value *from = VM()->stack_top - have;

      VM()->stack_top = VM()->frame->slots;

      close_upvalue(VM(), VM()->frame->slots);

      trim_return(VM(), from, have, VM()->frame->expected_results);

      if (--VM()->frame == VM()->call_stack) {
        gab_value module = POP();

        gab_gc_push_inc_if_obj_ref(GC(), module);

        return gab_run_success(module);
      }

      LOAD_FRAME();

      NEXT();
    }
    }

    {
      gab_value prop, index;
      i16 prop_offset;
      gab_obj_object *obj;

      CASE_CODE(GET_INDEX) : {
        prop = PEEK();
        index = PEEK2();

        if (!GAB_VAL_IS_OBJECT(index)) {
          STORE_FRAME();
          return gab_run_fail(VM(), "Only objects have properties");
        }

        obj = GAB_VAL_TO_OBJECT(index);

        prop_offset = gab_obj_shape_find(obj->shape, prop);

        DROP_N(2);

        goto complete_obj_get;
      }

      CASE_CODE(GET_PROPERTY) : {
        prop = READ_CONSTANT;
        index = PEEK();

        gab_obj_shape **cached_shape = READ_INLINECACHE(gab_obj_shape);
        prop_offset = READ_SHORT;

        if (!GAB_VAL_IS_OBJECT(index)) {
          STORE_FRAME();
          return gab_run_fail(VM(), "Only objects have properties");
        }

        obj = GAB_VAL_TO_OBJECT(index);

        if (*cached_shape == NULL || *cached_shape != obj->shape) {
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
      PUSH(gab_obj_object_get(obj, prop_offset));
      NEXT();
    }
    }

    {

      gab_value value, prop, index;
      i16 prop_offset;
      gab_obj_object *obj;

      CASE_CODE(SET_INDEX) : {
        // Leave these on the stack for now in case we collect.
        value = PEEK();
        prop = PEEK2();
        index = PEEK_N(3);

        gab_gc_push_inc_if_obj_ref(GC(), value);

        if (!GAB_VAL_IS_OBJECT(index)) {
          STORE_FRAME();
          return gab_run_fail(VM(), "Only objects have properties");
        }

        obj = GAB_VAL_TO_OBJECT(index);

        prop_offset = gab_obj_shape_find(obj->shape, prop);

        if (prop_offset < 0) {
          // The key didn't exist on the old shape.
          // Create a new shape and update the cache.
          gab_obj_shape *shape =
              gab_obj_shape_extend(obj->shape, ENGINE(), prop);

          // Update the obj and get the new offset.
          prop_offset = gab_obj_object_extend(obj, ENGINE(), shape, value);
        }

        // Now its safe to drop the three values
        DROP_N(2);

        goto complete_obj_set;
      }

      CASE_CODE(SET_PROPERTY) : {
        prop = READ_CONSTANT;
        value = PEEK();
        index = PEEK2();

        gab_gc_push_inc_if_obj_ref(GC(), value);

        gab_obj_shape **cached_shape = READ_INLINECACHE(gab_obj_shape);

        prop_offset = READ_SHORT;

        if (!GAB_VAL_IS_OBJECT(index)) {
          STORE_FRAME();
          return gab_run_fail(VM(), "Only objects have properties");
        }

        obj = GAB_VAL_TO_OBJECT(index);

        if (*cached_shape == NULL || *cached_shape != obj->shape) {
          // The cache hasn't been created yet.
          *cached_shape = obj->shape;

          prop_offset = gab_obj_shape_find(obj->shape, prop);
          // Writes into the short just before the ip.
          WRITE_SHORT(prop_offset);
        }

        if (prop_offset < 0) {
          // The key didn't exist on the old shape.
          // Create a new shape and update the cache.
          *cached_shape = gab_obj_shape_extend(*cached_shape, ENGINE(), prop);

          // Update the obj and get the new offset.
          prop_offset =
              gab_obj_object_extend(obj, ENGINE(), *cached_shape, value);

          // Write the offset.
          WRITE_SHORT(prop_offset);
        }

        DROP_N(2);

        PUSH(value);

        goto complete_obj_set;
      }

    complete_obj_set : {
      gab_gc_push_dec_if_obj_ref(GC(), gab_obj_object_get(obj, prop_offset));

      gab_obj_object_set(obj, prop_offset, value);

      NEXT();
    }
    }

    CASE_CODE(CONSTANT) : {
      PUSH(READ_CONSTANT);
      NEXT();
    }

    CASE_CODE(NEGATE) : {
      if (!GAB_VAL_IS_NUMBER(PEEK())) {
        STORE_FRAME();
        return gab_run_fail(VM(), "Can only negate numbers.");
      }
      PUSH(GAB_VAL_NUMBER(-GAB_VAL_TO_NUMBER(POP())));
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
      PUSH(GAB_VAL_BOOLEAN(gab_val_equal(POP(), POP())));
      NEXT();
    }

    CASE_CODE(SWAP) : {
      gab_value tmp = PEEK();
      PEEK() = PEEK2();
      PEEK2() = tmp;
      NEXT();
    }

    CASE_CODE(DUP) : {
      PUSH(PEEK());
      NEXT();
    }

    CASE_CODE(CONCAT) : {
      if (!GAB_VAL_IS_STRING(PEEK()) + !GAB_VAL_IS_STRING(PEEK2())) {
        STORE_FRAME();
        return gab_run_fail(VM(), "Can only concatenate strings");
      }

      gab_obj_string *b = GAB_VAL_TO_STRING(POP());
      gab_obj_string *a = GAB_VAL_TO_STRING(POP());

      gab_obj_string *obj = gab_obj_string_concat(ENGINE(), a, b);

      PUSH(GAB_VAL_OBJ(obj));

      NEXT();
    }

    CASE_CODE(STRINGIFY) : {
      if (!GAB_VAL_IS_STRING(PEEK())) {
        gab_obj_string *obj = gab_val_to_obj_string(POP(), ENGINE());

        PUSH(GAB_VAL_OBJ(obj));
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
      PUSH(GAB_VAL_BOOLEAN(gab_val_falsey(POP())));
      NEXT();
    }

    CASE_CODE(ASSERT) : {
      if (GAB_VAL_IS_NULL(PEEK())) {
        STORE_FRAME();
        return gab_run_fail(VM(), "Expected value to not be null");
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
      if (gab_val_equal(test, pattern)) {
        POP();
        PUSH(GAB_VAL_BOOLEAN(true));
      } else {
        PUSH(GAB_VAL_BOOLEAN(false));
      }
      NEXT();
    }

    CASE_CODE(SPREAD) : {
      u8 want = READ_BYTE;

      gab_value index = POP();

      if (!GAB_VAL_IS_OBJECT(index)) {
        STORE_FRAME();
        return gab_run_fail(VM(), "Spread operator only works on objects");
      }

      gab_obj_object *obj = GAB_VAL_TO_OBJECT(index);

      u8 have = obj->is_dynamic ? obj->dynamic_values.size : obj->static_size;

      trim_return(
          VM(), obj->is_dynamic ? obj->dynamic_values.data : obj->static_values,
          have, want);

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

    CASE_CODE(LOAD_UPVALUE_0) :
    CASE_CODE(LOAD_UPVALUE_1) :
    CASE_CODE(LOAD_UPVALUE_2) :
    CASE_CODE(LOAD_UPVALUE_3) :
    CASE_CODE(LOAD_UPVALUE_4) :
    CASE_CODE(LOAD_UPVALUE_5) :
    CASE_CODE(LOAD_UPVALUE_6) :
    CASE_CODE(LOAD_UPVALUE_7) :
    CASE_CODE(LOAD_UPVALUE_8) : {
      PUSH(*UPVALUE(INSTR() - OP_LOAD_UPVALUE_0)->data);
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
      *UPVALUE(INSTR()- OP_STORE_UPVALUE_0)->data = PEEK();
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
      PUSH(*UPVALUE(READ_BYTE)->data);
      NEXT();
    }

    CASE_CODE(STORE_UPVALUE) : {
      *UPVALUE(READ_BYTE)->data = PEEK();
      NEXT();
    }

    CASE_CODE(POP_STORE_UPVALUE) : {
      *UPVALUE(READ_BYTE)->data = POP();
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
      gab_obj_function *func = GAB_VAL_TO_FUNCTION(READ_CONSTANT);

      gab_obj_upvalue *upvalues[UPVALUE_MAX];

      for (int i = 0; i < func->nupvalues; i++) {
        u8 is_local = READ_BYTE;
        u8 index = READ_BYTE;

        if (is_local) {
          upvalues[i] =
              capture_upvalue(ENGINE(), VM(), VM()->frame->slots + index);
        } else {
          upvalues[i] = VM()->frame->closure->upvalues[index];
        }

        gab_gc_push_inc_obj_ref(GC(), (gab_obj *)upvalues[i]);
      }

      gab_obj_closure *obj = gab_obj_closure_create(ENGINE(), func, upvalues);

      PUSH(GAB_VAL_OBJ(obj));

      gab_gc_push_dec_obj_ref(GC(), (gab_obj *)obj);

      NEXT();
    }

    CASE_CODE(OBJECT) : {
      u8 size = READ_BYTE;

      for (u64 i = 0; i < size * 2; i++) {
        gab_gc_push_inc_if_obj_ref(ENGINE()->gc, PEEK_N(i));
      }

      gab_obj_shape *shape =
          gab_obj_shape_create(ENGINE(), VM()->stack_top - (size * 2), size, 2);

      gab_obj_object *obj = gab_obj_object_create(
          ENGINE(), shape, VM()->stack_top + 1 - (size * 2), size, 2);

      DROP_N(size * 2);

      PUSH(GAB_VAL_OBJ(obj));

      gab_gc_push_dec_obj_ref(GC(), (gab_obj *)obj);

      NEXT();
    }

    CASE_CODE(OBJECT_ARRAY) : {
      u8 size = READ_BYTE;

      for (u64 i = 0; i < size; i++) {
        gab_gc_push_inc_if_obj_ref(ENGINE()->gc, PEEK_N(i));
      }

      gab_obj_shape *shape = gab_obj_shape_create_array(ENGINE(), size);

      gab_obj_object *obj = gab_obj_object_create(
          ENGINE(), shape, VM()->stack_top - (size), size, 1);

      DROP_N(size);

      PUSH(GAB_VAL_OBJ(obj));

      gab_gc_push_dec_obj_ref(GC(), (gab_obj *)obj);

      NEXT();
    }
  }
  // This should be unreachable
  return NULL;
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OPERATION
