#include "import.h"
#include "engine.h"
#include "gab.h"
#include <dlfcn.h>

void gab_egimpputmod(struct gab_eg *gab, const char *name, gab_value mod,
                   a_gab_value *val) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = NEW(struct gab_imp);

  i->k = kGAB_IMPORT_SOURCE;
  i->cache = val;
  i->as.mod = mod;

  d_gab_imp_insert(&gab->imports, hash, i);
}

void gab_egimpputshd(struct gab_eg *gab, const char *name, void *obj,
                   a_gab_value *val) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = NEW(struct gab_imp);

  i->k = kGAB_IMPORT_SHARED;
  i->cache = val;
  i->as.shared = obj;

  if (val)
    gab_negkeep(gab, val->len, val->data);

  d_gab_imp_insert(&gab->imports, hash, i);
}

struct gab_imp *gab_egimpat(struct gab_eg *gab, const char *name) {
  uint64_t hash = s_char_hash(s_char_cstr(name), gab->hash_seed);

  struct gab_imp *i = d_gab_imp_read(&gab->imports, hash);

  return i;
}

a_gab_value *gab_impvals(struct gab_imp *imp) { return imp->cache; }

void gab_impdestroy(struct gab_eg *gab, struct gab_gc *gc, struct gab_imp *i) {
  switch (i->k) {
  case kGAB_IMPORT_SHARED:
    dlclose(i->as.shared);
    break;
  default:
    break;
  }

  if (i->cache)
    a_gab_value_destroy(i->cache);

  DESTROY(i);
}
