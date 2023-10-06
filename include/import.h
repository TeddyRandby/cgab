#ifndef GAB_IMPORT_H
#define GAB_IMPORT_H

#include "gab.h"

enum gab_import_k {
  IMPORT_SHARED,
  IMPORT_SOURCE,
};

struct gab_imp {
  enum gab_import_k k;

  union {
    gab_value mod;
    void *shared;
  } as;

  a_gab_value *cache;
};

/**
 * Destroy an import
 */
void gab_impdestroy(struct gab_eg *gab, struct gab_gc *gc, struct gab_imp *imp);

#endif
