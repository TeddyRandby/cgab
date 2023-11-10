#ifndef GAB_VM_H
#define GAB_VM_H

#include "include/gab.h"

/**
 * The run-time representation of a callframe.
 */
struct gab_vm_frame {
  struct gab_obj_block *b;

  /**
   *The instruction pointer.
   */
  uint8_t *ip;

  /**
   * The value on the stack where this callframe begins.
   */
  gab_value *slots;

  /**
   * Every call wants a different number of results.
   * This is set at the call site.
   */
  uint8_t want;
};

/*
 * The gab virtual machine. This has all the state needed for executing
 * bytecode.
 */
struct gab_vm {
  /*
   * The flags passed in to the vm
   */
  uint8_t flags;

  struct gab_vm_frame *fp;

  gab_value *sp;

  gab_value sb[cGAB_STACK_MAX];

  struct gab_vm_frame fb[cGAB_FRAMES_MAX];
};

void gab_vmcreate(struct gab_vm *vm, uint8_t flags, size_t argc,
                  gab_value argv[argc]);

void gab_vmdestroy(struct gab_eg *gab, struct gab_vm *vm);

a_gab_value *gab_vmrun(struct gab_triple gab, gab_value main, uint8_t flags,
                       size_t argc, gab_value argv[argc]);

gab_value gab_vm_panic(struct gab_triple gab, const char *msg);

int32_t gab_vm_push(struct gab_vm *vm, uint64_t argc, gab_value argv[argc]);

#endif
