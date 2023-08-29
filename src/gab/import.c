#include "include/import.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include <dlfcn.h>
#include <stdio.h>

uint64_t gab_impmod(gab_eg *gab, const char *name, gab_value mod, a_gab_value *val) {
  uint64_t hash = s_int8_t_hash(s_int8_t_cstr(name), gab->hash_seed);

  gab_imp *i = NEW(gab_imp);

  i->k = IMPORT_SOURCE;
  i->cache = val;
  i->as.mod = mod;

  gab_egkeep(gab, mod);

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);

  return hash;
}

uint64_t gab_impshd(gab_eg *gab, const char *name, void *obj, a_gab_value *val) {
  uint64_t hash = s_int8_t_hash(s_int8_t_cstr(name), gab->hash_seed);

  gab_imp *i = NEW(gab_imp);

  i->k = IMPORT_SHARED;
  i->cache = val;
  i->as.shared = obj;

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);

  return hash;
}

a_gab_value *gab_imphas(gab_eg *gab, const char *name) {
  uint64_t hash = s_int8_t_hash(s_int8_t_cstr(name), gab->hash_seed);

  gab_imp *i = d_gab_imp_read(&gab->imports, hash);

  return i ? i->cache: NULL;
}

void gab_import_destroy(gab_eg *gab, gab_gc *gc, gab_imp *i) {
  switch (i->k) {
  case IMPORT_SHARED:
    dlclose(i->as.shared);
    break;
  default:
    break;
  }

  a_gab_value_destroy(i->cache);

  DESTROY(i);
}

void gab_impdestroy(gab_eg *gab, gab_gc *gc) {
  for (uint64_t i = 0; i < gab->imports.cap; i++) {
    if (d_gab_imp_iexists(&gab->imports, i)) {
      gab_import_destroy(gab, gc, d_gab_imp_ival(&gab->imports, i));
    }
  }
}
