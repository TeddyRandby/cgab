#ifndef BLUF_VM_H
#define BLUF_VM_H

#include "../../common/common.h"
#include "../compiler/compiler.h"
#include "../compiler/object.h"
#include "gc.h"
/*
  A run-time representation of a callframe.
*/
typedef struct gab_call_frame gab_call_frame;
struct gab_call_frame {
  /*
    The closure being called.
  */
  gab_obj_closure *closure;
  /*
    The instruction pointer.
    This is stored and loaded by the macros STORE_FRAME and LOAD_FRAME.
  */
  u8 *ip;
  /*
    The value on the stack where this callframe begins.
    Locals are offset from this.
  */
  gab_value *slots;
  /*
    Every call expects a certain number of results.
    This is set during when the values are called.
  */
  u8 expected_results;
};

typedef struct gab_vm gab_vm;
struct gab_vm {

  /*
    Upvalues to close when the current function returns.
  */
  gab_obj_upvalue *open_upvalues;

  /*
    The value stack.
  */
  gab_value *stack_top;

  /*
   The current frame
  */
  gab_call_frame *frame;

  gab_value stack[STACK_MAX];

  /*
    The call stack.
  */
  gab_call_frame call_stack[FRAMES_MAX];
};

/*
  Run a gab function.

  A value of 0 means an error was encountered.
*/
gab_result *gab_engine_run(gab_engine *eng, gab_obj_function *func);

#endif
