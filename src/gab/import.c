#include "include/import.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include <dlfcn.h>
#include <stdio.h>

uint64_t gab_impmod(struct gab_eg *gab, const char *name, gab_value mod,
                    a_gab_value *val) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = NEW(struct gab_imp);

  i->k = IMPORT_SOURCE;
  i->cache = val;
  i->as.mod = mod;

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);

  return hash;
}

uint64_t gab_impshd(struct gab_eg *gab, const char *name, void *obj,
                    a_gab_value *val) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = NEW(struct gab_imp);

  i->k = IMPORT_SHARED;
  i->cache = val;
  i->as.shared = obj;

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);

  return hash;
}

a_gab_value *gab_imphas(struct gab_eg *gab, const char *name) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = d_gab_imp_read(&gab->imports, hash);

  return i ? i->cache : NULL;
}

void gab_import_destroy(struct gab_eg *gab, struct gab_gc *gc, struct gab_imp *i) {
  switch (i->k) {
  case IMPORT_SHARED:
    dlclose(i->as.shared);
    break;
  default:
    break;
  }

  if (i->cache)
    a_gab_value_destroy(i->cache);

  DESTROY(i);
}

void gab_impdestroy(struct gab_eg *gab, struct gab_gc *gc) {
  for (uint64_t i = 0; i < gab->imports.cap; i++) {
    if (d_gab_imp_iexists(&gab->imports, i)) {
      gab_import_destroy(gab, gc, d_gab_imp_ival(&gab->imports, i));
    }
  }
}
