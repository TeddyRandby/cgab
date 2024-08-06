#include "gab.h"
#include <stdio.h>

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  gab_value map = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(map) != kGAB_MAP)
    return gab_pktypemismatch(gab, map, kGAB_MAP);

  gab_value val = gab_mapat(map, key);

  if (val == gab_undefined)
    gab_vmpush(gab.vm, gab_none);
  else
    gab_vmpush(gab.vm, gab_ok, val);

  return nullptr;
}

a_gab_value *gab_lib_del(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  gab_value map = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(map) != kGAB_MAP)
    return gab_pktypemismatch(gab, map, kGAB_MAP);

  gab_value new_map = gab_mapdel(gab, map, key);

  gab_vmpush(gab.vm, new_map);

  return nullptr;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value map = gab_arg(0);
  gab_value key = gab_arg(1);
  gab_value val = gab_arg(2);

  if (gab_valkind(map) != kGAB_MAP)
    return gab_pktypemismatch(gab, map, kGAB_MAP);

  gab_vmpush(gab.vm, gab_mapput(gab, map, key, val));

  return nullptr;
}

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value map = gab_arg(0);

  if (gab_valkind(map) != kGAB_MAP)
    return gab_pktypemismatch(gab, map, kGAB_MAP);

  gab_vmpush(gab.vm, gab_number(gab_maplen(map)));

  return nullptr;
}

a_gab_value *gab_lib_next(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value map = gab_arg(0);

  if (gab_valkind(map) != kGAB_MAP)
    return gab_pktypemismatch(gab, map, kGAB_MAP);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value map_t = gab_egtype(gab.eg, kGAB_MAP);

  struct gab_spec_argt specs[] = {
      {
          "put!",
          map_t,
          gab_snative(gab, "gab.put!", gab_lib_put),
      },
      {
          "del!",
          map_t,
          gab_snative(gab, "gab.del!", gab_lib_del),
      },
      {
          "at",
          map_t,
          gab_snative(gab, "gab.at", gab_lib_at),
      },
      {
          "next",
          map_t,
          gab_snative(gab, "gab.next", gab_lib_next),
      },
      {
          "len",
          map_t,
          gab_snative(gab, "gab.len", gab_lib_len),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return a_gab_value_one(map_t);
}
