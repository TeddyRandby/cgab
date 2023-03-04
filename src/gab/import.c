#include "include/import.h"
#include "include/engine.h"
#include <dlfcn.h>

u64 gab_imports_module(gab_engine *gab, s_i8 name, gab_module *mod,
                       gab_value val) {
  u64 hash = s_i8_hash(name, gab->hash_seed);

  gab_import *i = NEW(gab_import);

  i->k = IMPORT_SOURCE;
  i->cache = val;
  i->as.mod = mod;

  d_gab_import_insert(&gab->imports, hash, i);

  return hash;
}

u64 gab_imports_shared(gab_engine *gab, s_i8 name, void *obj, gab_value val) {
  u64 hash = s_i8_hash(name, gab->hash_seed);

  gab_import *i = NEW(gab_import);

  i->k = IMPORT_SHARED;
  i->cache = val;
  i->as.shared = obj;

  d_gab_import_insert(&gab->imports, hash, i);

  return hash;
}

gab_value gab_imports_exists(gab_engine* gab, s_i8 name) {
  u64 hash = s_i8_hash(name, gab->hash_seed);

  if (d_gab_import_exists(&gab->imports, hash)) {
    gab_import *i = d_gab_import_read(&gab->imports, hash);
    return i->cache;
  }

  return GAB_VAL_UNDEFINED();

}

void gab_import_collect(gab_engine *gab, gab_import *i) {
  gab_dref(gab, NULL, i->cache);
}

void gab_import_destroy(gab_engine* gab, gab_import* i) {
  switch (i->k) {
  case IMPORT_SHARED:
    dlclose(i->as.shared);
  default:
    break;
  }
  DESTROY(i);
}

void gab_imports_collect(gab_engine *gab) {
  for (u64 i = 0; i < gab->imports.cap; i++) {
    if (d_gab_import_iexists(&gab->imports, i)) {
      gab_import_collect(gab, d_gab_import_ival(&gab->imports, i));
    }
  }
}

void gab_imports_destroy(gab_engine *gab) {
  for (u64 i = 0; i < gab->imports.cap; i++) {
    if (d_gab_import_iexists(&gab->imports, i)) {
      gab_import_destroy(gab, d_gab_import_ival(&gab->imports, i));
    }
  }
}