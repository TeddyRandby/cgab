#ifndef GAB_GC_H
#define GAB_GC_H

#include "gab.h"

typedef struct gab_eg gab_eg;
typedef struct gab_obj gab_obj;
typedef struct gab_vm gab_vm;

#if cGAB_LOG_GC

typedef struct rc_update {
  const char *file;
  int32_t line;
  gab_obj *val;
} rc_update;

#define T rc_update
#include "include/vector.h"

#define NAME rc_tracker
#define K gab_obj *
#define V int32_t
#define DEF_V 0
#define HASH(a) ((uint64_t)a)
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

#endif

#define T gab_obj *
#define NAME gab_obj
#include "vector.h"

typedef struct gab_gc {
  uint64_t nincrements;
  uint64_t ndecrements;
  uint64_t nroots;

#if cGAB_LOG_GC
  v_rc_update tracked_increments;
  v_rc_update tracked_decrements;

  d_rc_tracker tracked_values;
#endif

  v_gab_obj garbage;

  gab_obj *roots[cGAB_GC_ROOT_BUFF_MAX];
  gab_obj *increments[cGAB_GC_INC_BUFF_MAX];
  gab_obj *decrements[cGAB_GC_DEC_BUFF_MAX];
} gab_gc;

typedef void (*gab_gc_visitor)(gab_gc *gc, gab_obj *obj);

void gab_gc_create(gab_gc *vm);

void gab_gc_destroy(gab_gc *vm);

void gab_gc_run(gab_gc *gc, gab_vm *vm);

#if cGAB_LOG_GC

void __gab_gc_iref(gab_gc *gc, gab_vm *vm, gab_value val, const char *file,
                   int32_t line);
void __gab_gc_dref(gab_gc *gc, gab_vm *vm, gab_value val, const char *file,
                   int32_t line);

#define gab_gc_iref(gc, vm, val)                                               \
  (__gab_gc_iref(gc, vm, val, __FILE__, __LINE__))
#define gab_gc_dref(gc, vm, val)                                               \
  (__gab_gc_dref(gc, vm, val, __FILE__, __LINE__))

#else

#endif

#endif
