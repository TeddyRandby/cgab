#ifndef GAB_GC_H
#define GAB_GC_H

#include "gab.h"

#define T struct gab_obj *
#define NAME gab_obj
#include "vector.h"

struct gab_gc {
  size_t nmodifications;
  size_t ndecrements;

  struct gab_obj *decrements[cGAB_GC_DEC_BUFF_MAX * 2];
  struct gab_obj *modifications[cGAB_GC_MOD_BUFF_MAX * 2];
};

typedef void (*gab_gc_visitor)(struct gab_triple gab, struct gab_obj *obj);

void gab_gccreate(struct gab_gc *gc);

void gab_gcdestroy(struct gab_gc *gc);

#endif
