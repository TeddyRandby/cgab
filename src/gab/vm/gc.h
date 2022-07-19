#ifndef GAB_GC_H
#define GAB_GC_H

#include "../compiler/object.h"

typedef struct gab_gc gab_gc;

struct gab_gc {
  u16 increment_count;
  u16 decrement_count;
  u16 root_count;

  d_u64 queue;
  d_u64 roots;

  gab_obj *increments[INC_DEC_MAX];
  gab_obj *decrements[INC_DEC_MAX];
};

#endif
