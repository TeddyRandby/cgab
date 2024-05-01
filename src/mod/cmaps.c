#include "map.h"
#include <stdio.h>

a_gab_value *gab_lib_new(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value map = map_create(gab, 0, 0, nullptr, nullptr);

    gab_vmpush(gab.vm, map);
    return nullptr;
  }

  case 2: {
    if (gab_valkind(argv[1]) != kGAB_RECORD) {
      return gab_panic(gab, "&:new expects a record as second argument");
    }

    gab_value shp = gab_recshp(argv[1]);

    size_t len = gab_reclen(argv[1]);

    gab_value map =
        map_create(gab, len, 1, gab_shpdata(shp), gab_recdata(argv[1]));

    gab_vmpush(gab.vm, map);
    return nullptr;
  }

  default:
    return gab_panic(gab, "&:new expects 1 or 2 arguments");
  }
}

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "&:len expects 1 argument");
  }

  d_uint64_t *data = gab_boxdata(argv[0]);

  gab_value res = gab_number(data->len);

  gab_vmpush(gab.vm, res);
  return nullptr;
}

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 2) {
    return gab_panic(gab, "&:at expects 2 arguments");
  }

  gab_value res = map_at(argv[0], argv[1]);

  if (res == gab_undefined) {
    gab_vmpush(gab.vm, gab_sigil(gab, "none"));
    return nullptr;
  }

  gab_vmpush(gab.vm, gab_sigil(gab, "ok"));
  gab_vmpush(gab.vm, res);
  return nullptr;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 3) {
    return gab_panic(gab, "&:put! expects 3 arguments");
  }

  map_put(gab, argv[0], argv[1], argv[2]);

  gab_vmpush(gab.vm, *argv);
  return nullptr;
}

a_gab_value *gab_lib_add(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 3) {
    return gab_panic(gab, "&:add! expects 3 arguments");
  }

  if (map_has(argv[0], argv[1])) {
    gab_vmpush(gab.vm, gab_string(gab, "KEY_ALREADY_EXISTS"));
    return nullptr;
  }

  map_put(gab, argv[0], argv[1], argv[2]);

  gab_vmpush(gab.vm, gab_sigil(gab, "ok"));
  return nullptr;
}

a_gab_value *gab_lib_next(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  d_uint64_t *map = gab_boxdata(argv[0]);

  gab_value key = gab_arg(1);

  uint64_t next_index;

  if (key == gab_sigil(gab, "next.init")) {
    next_index = d_uint64_t_inext(map, 0);
  } else {
    uint64_t index = d_uint64_t_index_of(map, key);
    next_index = d_uint64_t_inext(map, index + 1);
  }

  gab_value res[] = {gab_none, gab_nil, gab_nil};

  if (next_index == -1) {
    goto fin;
  }

  res[0] = gab_ok;
  res[1] = d_uint64_t_ikey(map, next_index);
  res[1] = d_uint64_t_ival(map, next_index);

fin:
  gab_nvmpush(gab.vm, 3, res);
  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_string(gab, "gab.map");

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_sigil(gab, "map"),
          gab_snative(gab, "map", gab_lib_new),
      },
      {
          "len",
          type,
          gab_snative(gab, "len", gab_lib_len),
      },
      {
          "put!",
          type,
          gab_snative(gab, "put!", gab_lib_put),
      },
      {
          "add!",
          type,
          gab_snative(gab, "add!", gab_lib_add),
      },
      {
          "at",
          type,
          gab_snative(gab, "at", gab_lib_at),
      },
      {
          "next",
          type,
          gab_snative(gab, "next", gab_lib_next),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return a_gab_value_one(type);
}
