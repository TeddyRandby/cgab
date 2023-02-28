#ifndef GAB_VM_H
#define GAB_VM_H
#include "value.h"

typedef struct gab_obj_block gab_obj_block;
typedef struct gab_obj_upvalue gab_obj_upvalue;
typedef struct gab_engine gab_engine;
typedef struct gab_module gab_module;

/*
  A run-time representation of a callframe.
*/
typedef struct gab_vm_frame {
  gab_obj_block *c;

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
} gab_vm_frame;

gab_value gab_vm_run(gab_engine *gab, gab_module *main, u8 flags, u8 argc,
                     gab_value argv[argc]);

gab_value gab_vm_panic(gab_engine *gab, const char *msg);

void gab_vm_stack_dump(gab_engine *gab);

void gab_vm_frame_dump(gab_engine *gab, u64 value);

#endif
