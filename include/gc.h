#ifndef GAB_GC_H
#define GAB_GC_H

#include "include/value.h"

typedef struct gab_engine gab_engine;
typedef struct gab_obj gab_obj;

#if GAB_LOG_GC

typedef struct rc_update {
  const char *file;
  i32 line;
  gab_obj *val;
} rc_update;

#define T rc_update
#include "include/vector.h"

#define NAME rc_tracker
#define K gab_obj *
#define V i32
#define DEF_V 0
#define HASH(a) ((u64)a)
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

#endif

typedef struct gab_gc {
  u64 nincrements;
  u64 ndecrements;
  u64 nroots;

#if GAB_LOG_GC
  v_rc_update tracked_increments;
  v_rc_update tracked_decrements;

  d_rc_tracker tracked_values;
#endif

  gab_obj *roots[GC_ROOT_BUFF_MAX];
  gab_obj *increments[GC_INC_BUFF_MAX];
  gab_obj *decrements[GC_DEC_BUFF_MAX];
} gab_gc;

void gab_gc_create(gab_engine *gab);

void gab_gc_destroy(gab_engine *gab);

void gab_gc_collect(gab_engine *gab);

void gab_gc_iref_many(gab_engine *gab, u64 len, gab_value values[len]);

void gab_gc_dref_many(gab_engine *gab, u64 len, gab_value values[len]);

#if GAB_LOG_GC

void __gab_gc_iref(gab_engine *gab, gab_value val, const char *file, i32 line);
void __gab_gc_dref(gab_engine *gab, gab_value val, const char *file, i32 line);

#define gab_gc_iref(gab, gc, val) (__gab_gc_iref(gab, val, __FILE__, __LINE__))
#define gab_gc_dref(gab, gc, val) (__gab_gc_dref(gab, val, __FILE__, __LINE__))

#else

void gab_gc_iref(gab_engine *gab, gab_value val);

void gab_gc_dref(gab_engine *gab, gab_value val);

#endif

#endif
