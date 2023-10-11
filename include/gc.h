#ifndef GAB_GC_H
#define GAB_GC_H

#include "gab.h"

#define T struct gab_obj *
#define NAME gab_obj
#include "vector.h"

struct gab_gc {
  size_t nmodifications;
  size_t ndecrements;
  size_t nroots;

  struct gab_obj *roots[cGAB_GC_ROOT_BUFF_MAX];
  struct gab_obj *decrements[cGAB_GC_DEC_BUFF_MAX];
  struct gab_obj *modifications[cGAB_GC_MOD_BUFF_MAX];
};

typedef void (*gab_gc_visitor)(struct gab_eg *gab, struct gab_gc *gc,
                               struct gab_vm *vm, struct gab_obj *obj);

void gab_gccreate(struct gab_gc *gc);

void gab_gcdestroy(struct gab_gc *gc);

#endif
