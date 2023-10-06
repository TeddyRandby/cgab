#include "include/import.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/gc.h"
#include <dlfcn.h>
#include <stdio.h>

void gab_impputmod(struct gab_eg *gab, const char *name, gab_value mod,
                   a_gab_value *val) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = NEW(struct gab_imp);

  i->k = IMPORT_SOURCE;
  i->cache = val;
  i->as.mod = mod;

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);
}

void gab_impputshd(struct gab_eg *gab, const char *name, void *obj,
                   a_gab_value *val) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = NEW(struct gab_imp);

  i->k = IMPORT_SHARED;
  i->cache = val;
  i->as.shared = obj;

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);
}

struct gab_imp *gab_impat(struct gab_eg *gab, const char *name) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = d_gab_imp_read(&gab->imports, hash);

  return i;
}

a_gab_value *gab_impval(struct gab_imp *imp) { return imp->cache; }

void gab_impdestroy(struct gab_eg *gab, struct gab_gc *gc, struct gab_imp *i) {
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
