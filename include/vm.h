#ifndef GAB_VM_H
#define GAB_VM_H

#include "gab.h"

/*
  A run-time representation of a callframe.
*/
typedef struct gab_call_frame {
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
} gab_call_frame;

struct gab_vm {

  /*
    Upvalues to close when the current function returns.
  */
  gab_obj_upvalue *open_upvalues;

  /*
   The current frame
  */
  gab_call_frame *frame;

  gab_value *top;

  gab_value stack[STACK_MAX];

  /*
    The call stack.
  */
  gab_call_frame call_stack[FRAMES_MAX];
};

void gab_vm_create(gab_vm *vm);

gab_result gab_vm_run(gab_engine *gab, gab_value closure);

#endif
