#include "gab.h"
#include <stdio.h>

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value val = gab_recat(rec, key);

  if (val == gab_undefined)
    gab_vmpush(gab.vm, gab_none);
  else
    gab_vmpush(gab.vm, gab_ok, val);

  return nullptr;
}

a_gab_value *gab_lib_del(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value new_rec = gab_recdel(gab, rec, key);

  gab_vmpush(gab.vm, new_rec);

  return nullptr;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);
  gab_value val = gab_arg(2);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_vmpush(gab.vm, gab_recput(gab, rec, key, val));

  return nullptr;
}

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_vmpush(gab.vm, gab_number(gab_reclen(rec)));

  return nullptr;
}

a_gab_value *gab_lib_next(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  size_t len = gab_reclen(rec);

  if (len == 0)
    goto fin;

  if (key == gab_sigil(gab, "next.init")) {
    gab_vmpush(gab.vm, gab_ok, gab_ukrecat(rec, 0), gab_uvrecat(rec, 0));
    return nullptr;
  }

  size_t i = gab_recfind(rec, key);

  if (i == -1 || i + 1 == len)
    goto fin;

  gab_vmpush(gab.vm, gab_ok, gab_ukrecat(rec, i + 1), gab_uvrecat(rec, i + 1));
  return nullptr;

fin:
  gab_vmpush(gab.vm, gab_none);
  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value rec_t = gab_egtype(gab.eg, kGAB_RECORD);

  struct gab_spec_argt specs[] = {
      {
          "put!",
          rec_t,
          gab_snative(gab, "gab.put!", gab_lib_put),
      },
      {
          "del!",
          rec_t,
          gab_snative(gab, "gab.del!", gab_lib_del),
      },
      {
          "at",
          rec_t,
          gab_snative(gab, "gab.at", gab_lib_at),
      },
      {
          "next",
          rec_t,
          gab_snative(gab, "gab.next", gab_lib_next),
      },
      {
          "len",
          rec_t,
          gab_snative(gab, "gab.len", gab_lib_len),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return a_gab_value_one(rec_t);
}
