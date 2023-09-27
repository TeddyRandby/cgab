#ifndef GAB_GC_H
#define GAB_GC_H

#include "gab.h"

#if cGAB_LOG_GC

typedef struct rc_update {
  const char *file;
  int32_t line;
  struct gab_obj *val;
} rc_update;

#define T rc_update
#include "include/vector.h"

#define NAME rc_tracker
#define K struct gab_obj *
#define V int32_t
#define DEF_V 0
#define HASH(a) ((uint64_t)a)
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

#endif

#define T struct gab_obj *
#define NAME gab_obj
#include "vector.h"

struct gab_gc {
  uint64_t nincrements;
  uint64_t ndecrements;
  uint64_t nroots;

  v_gab_obj garbage;

  struct gab_obj *roots[cGAB_GC_ROOT_BUFF_MAX];
  struct gab_obj *increments[cGAB_GC_INC_BUFF_MAX];
  struct gab_obj *decrements[cGAB_GC_DEC_BUFF_MAX];
};

typedef void (*gab_gc_visitor)(struct gab_gc *gc, struct gab_obj *obj);

void gab_gccreate(struct gab_gc *gc);

void gab_gcdestroy(struct gab_gc *gc);

#endif
