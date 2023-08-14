#ifndef GAB_IMPORT_H
#define GAB_IMPORT_H

#include "gab.h"

typedef enum {
  IMPORT_SHARED,
  IMPORT_SOURCE,
} gab_import_k;

typedef struct {
  gab_import_k k;

  union {
    gab_value mod;
    void *shared;
  } as;

  a_gab_value *cache;
} gab_imp;

#define NAME gab_imp
#define K u64
#define V gab_imp *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "dict.h"

/**
 * Allocate a new import for a gab_value module
 */
u64 gab_impmod(gab_eg *gab, const char *name, gab_value mod, a_gab_value* val);

/**
 * Allocate a new import for a shared object
 */
u64 gab_impshd(gab_eg *gab, const char *name, void *obj, a_gab_value* val);

/**
 * Check if an import exists
 */
a_gab_value* gab_imphas(gab_eg *gab, const char *name);

/**
 * Destroy an import
 */
void gab_impdestroy(gab_eg *gab, gab_gc *gc);

#endif
