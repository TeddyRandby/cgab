#ifndef BLUF_GC_H
#define BLUF_GC_H

#include "../../common/common.h"
#include "../compiler/object.h"

typedef struct gab_engine gab_engine;
typedef struct gab_vm gab_vm;
typedef struct gab_gc gab_gc;

struct gab_gc {
  gab_engine *eng;
  d_u64 queue;
  d_u64 roots;

  u16 increment_count;
  u16 decrement_count;
  u16 root_count;
  boolean active;

  gab_obj *increments[INC_DEC_MAX];
  gab_obj *decrements[INC_DEC_MAX];
};

gab_gc *gab_gc_create(gab_engine *eng);
void gab_gc_destroy(gab_gc *self);
void gab_gc_collect(gab_gc *gc);

/*
  Since modules handle garbage collection, they own all objects.

  Therefore, allocation of objects (at (run/compile)-time) must go through the
  module.
*/
void *gab_gc_reallocate(gab_gc *self, void *loc, u64 old_size, u64 new_size);

static inline void gab_gc_push_inc_obj_ref(gab_gc *self, gab_obj *obj) {
  ASSERT_TRUE(obj != NULL, "Don't try to gc null");
  self->increments[self->increment_count++] = obj;
  if (self->increment_count == INC_DEC_MAX) {
    gab_gc_collect(self);
  }
}

static inline void gab_gc_push_dec_obj_ref(gab_gc *self, gab_obj *obj) {
  ASSERT_TRUE(obj != NULL, "Don't try to gc null");
  if (self->decrement_count == INC_DEC_MAX) {
    gab_gc_collect(self);
  }
  self->decrements[self->decrement_count++] = obj;
}

static inline void gab_gc_push_dec_if_obj_ref(gab_gc *self, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj)) {
    gab_gc_push_dec_obj_ref(self, GAB_VAL_TO_OBJ(obj));
  }
}

static inline void gab_gc_push_inc_if_obj_ref(gab_gc *self, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj)) {
    gab_gc_push_inc_obj_ref(self, GAB_VAL_TO_OBJ(obj));
  }
}

#endif
