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
  gab_value cache;
} gab_import ;

#define NAME gab_import
#define K u64
#define V gab_import *
#define DEF_V NULL
#define HASH(a) (a)
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

u64 gab_imports_module(gab_engine* gab, s_i8 name, gab_value mod, gab_value val);

u64 gab_imports_shared(gab_engine* gab, s_i8 name, void* obj, gab_value val);

gab_value gab_imports_exists(gab_engine* gab, s_i8 name);

void gab_imports_destroy(gab_engine* gab);

#endif
