#ifndef GAB_IMPORT_H
#define GAB_IMPORT_H

#include "gab.h"

typedef enum {
  IMPORT_SHARED,
  IMPORT_SOURCE,
} gab_import_k;

struct gab_imp {
  gab_import_k k;

  union {
    gab_value mod;
    void *shared;
  } as;

  a_gab_value *cache;
};

#define NAME gab_imp
#define K uint64_t
#define V struct gab_imp *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

/**
 * Allocate a new import for a gab_value module
 */
uint64_t gab_impmod(struct gab_eg *gab, const char *name, gab_value mod, a_gab_value* val);

/**
 * Allocate a new import for a shared object
 */
uint64_t gab_impshd(struct gab_eg *gab, const char *name, void *obj, a_gab_value* val);

/**
 * Check if an import exists
 */
a_gab_value* gab_imphas(struct gab_eg *gab, const char *name);

/**
 * Destroy an import
 */
void gab_impdestroy(struct gab_eg *gab, struct gab_gc *gc);

#endif
