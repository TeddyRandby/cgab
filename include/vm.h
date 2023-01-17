#ifndef GAB_VM_H
#define GAB_VM_H

#include "gc.h"
#include "object.h"
#include <threads.h>

/*
  A run-time representation of a callframe.
*/
typedef struct gab_call_frame {
  gab_obj_closure *c;

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
  u8 want;
} gab_call_frame;

typedef struct gab_vm {
  /*
    Upvalues to close when the current function returns.
  */
  gab_obj_upvalue *open_upvalues;

  gab_call_frame *frame;

  gab_value *top;

  gab_value stack[STACK_MAX];

  /*
    The call stack.
  */
  gab_call_frame call_stack[FRAMES_MAX];
} gab_vm;

void gab_vm_create(gab_vm *vm);
void gab_vm_destroy(gab_vm *vm);

gab_value gab_vm_run(gab_engine *gab, gab_module *main, u8 argc,
                     gab_value argv[argc]);

#endif
