#include "map.h"
#include <assert.h>
#include <stdio.h>

void gab_lib_new(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value map = map_create(gab, 0, 0, NULL, NULL);

    gab_vmpush(gab.vm, map);

    return;
  }

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_RECORD) {
      gab_panic(gab, "&:new expects a record as second argument");
      return;
    }

    gab_value shp = gab_recshp(argv[1]);

    size_t len = gab_reclen(argv[1]);

    gab_value map =
        map_create(gab, len, 1, gab_shpdata(shp), gab_recdata(argv[1]));

    gab_vmpush(gab.vm, map);

    return;
  }

  default:
    gab_panic(gab, "&:new expects 1 or 2 arguments");
    return;
  }
}

void gab_lib_len(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, "&:len expects 1 argument");
    return;
  }

  d_gab_value *data = gab_boxdata(argv[0]);

  gab_value res = gab_number(data->len);

  gab_vmpush(gab.vm, res);

  return;
}

void gab_lib_at(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, "&:at expects 2 arguments");
    return;
  }

  gab_value res = map_at(argv[0], argv[1]);

  if (res == gab_undefined) {
    gab_vmpush(gab.vm, gab_string(gab.eg, "none"));
    return;
  }

  gab_vmpush(gab.vm, gab_string(gab.eg, "some"));
  gab_vmpush(gab.vm, res);
  return;
}

void gab_lib_put(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 3) {
    gab_panic(gab, "&:put! expects 3 arguments");
    return;
  }

  map_put(argv[0], argv[1], argv[2]);

  gab_vmpush(gab.vm, *argv);

  return;
}

void gab_lib_next(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  d_gab_value *map = gab_boxdata(argv[0]);

  switch (argc) {

  case 1: {
    uint64_t next_index = d_gab_value_inext(map, 0);

    if (next_index == -1) {
      gab_value res = gab_nil;

      gab_vmpush(gab.vm, res);

      return;
    }

    gab_value res = d_gab_value_ikey(map, next_index);

    gab_vmpush(gab.vm, res);

    return;
  }

  case 2: {

    gab_value key = argv[1];

    uint64_t index = d_gab_value_index_of(map, key);

    uint64_t next_index = d_gab_value_inext(map, index + 1);

    if (next_index == -1) {
      gab_value res = gab_nil;

      gab_vmpush(gab.vm, res);

      return;
    }

    gab_value res = d_gab_value_ikey(map, next_index);

    gab_vmpush(gab.vm, res);

    return;
  }

  default:
    gab_panic(gab, "&:next expects 1 or 2 arguments");
    return;
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_string(gab.eg, "Map");

  struct gab_spec_argt specs[] = {
      {
          "map",
          gab_nil,
          gab_sbuiltin(gab.eg, "new", gab_lib_new),
      },
      {
          "len",
          type,
          gab_sbuiltin(gab.eg, "len", gab_lib_len),
      },
      {
          "put!",
          type,
          gab_sbuiltin(gab.eg, "put", gab_lib_put),
      },
      {
          "at",
          type,
          gab_sbuiltin(gab.eg, "at", gab_lib_at),
      },
      {
          "next",
          type,
          gab_sbuiltin(gab.eg, "next", gab_lib_next),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
