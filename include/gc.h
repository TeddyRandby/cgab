#ifndef GAB_GC_H
#define GAB_GC_H

#include "gab.h"

typedef struct gab_vm gab_vm;

typedef struct gab_gc {
  i32 increment_count;
  i32 decrement_count;
  i32 root_count;

  d_u64 queue;
  d_u64 roots;

  gab_obj *increments[INC_DEC_MAX];
  gab_obj *decrements[INC_DEC_MAX];
} gab_gc;

void gab_gc_create(gab_gc *gc);
void gab_gc_destroy(gab_gc *gc);

void gab_gc_collect(gab_gc *gc, gab_vm* vm);

void gab_gc_iref(gab_gc *gc, gab_vm* vm, gab_value val);
void gab_gc_dref(gab_gc *gc, gab_vm* vm, gab_value val);
#endif
