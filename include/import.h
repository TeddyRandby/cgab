#ifndef GAB_IMPORT_H
#define GAB_IMPORT_H

#include "gab.h"
#include <stdint.h>

enum {
  kGAB_IMPORT_SHARED,
  kGAB_IMPORT_SOURCE,
};

struct gab_imp {
  uint8_t  k;

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
