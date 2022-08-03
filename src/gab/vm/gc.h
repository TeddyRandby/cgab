#ifndef GAB_GC_H
#define GAB_GC_H

#include "../compiler/object.h"

typedef struct gab_gc gab_gc;
struct gab_gc {
  i32 increment_count;
  i32 decrement_count;
  i32 root_count;

  d_u64 queue;
  d_u64 roots;

  gab_obj *increments[INC_DEC_MAX];
  gab_obj *decrements[INC_DEC_MAX];
};

void gab_gc_create(gab_gc *);

#endif
