#include "core.h"
#include "gab.h"
#include "list.h"
#include "map.h"

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))
a_gab_value *gab_lib_slice(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  if (gab_valkind(argv[0]) != kGAB_RECORD) {
    return gab_panic(gab, "Invalid call to gab_lib_slice");
  }

  gab_value rec = argv[0];

  uint64_t len = gab_reclen(rec);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2:
    if (gab_valkind(argv[1]) == kGAB_NUMBER) {
      uint64_t a = gab_valton(argv[1]);
      end = MIN(a, len);
      break;
    }

  case 3: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER ||
        gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to gab_lib_slice");
    }

    uint64_t a = gab_valton(argv[1]);
    start = MAX(a, 0);

    uint64_t b = gab_valton(argv[2]);
    end = MIN(b, len);
    break;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_slice");
  }

  uint64_t result_len = end - start;

  gab_value *keys = gab_shpdata(gab_recshp(rec)) + start;
  gab_value *values = gab_recdata(rec) + start;

  gab_value result = gab_record(gab, result_len, keys, values);

  gab_vmpush(gab.vm, result);

  return nullptr;
}

#undef MIN
#undef MAX
#undef CLAMP

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 2) {
    return gab_panic(gab, "Invalid call to  gab_lib_at");
  }

  gab_value result = gab_recat(argv[0], argv[1]);

  if (result == gab_undefined) {
    gab_vmpush(gab.vm, gab_sigil(gab, "none"));
    return nullptr;
  }

  gab_vmpush(gab.vm, gab_sigil(gab, "ok"));
  gab_vmpush(gab.vm, result);
  return nullptr;
}

a_gab_value *gab_lib_at_index(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  if (argc != 2) {
    return gab_panic(gab, "Invalid call to  gab_lib_at");
  }

  gab_value rec = gab_arg(0);
  gab_value idx = gab_arg(1);

  if (gab_valkind(idx) != kGAB_NUMBER) {
    return gab_pktypemismatch(gab, idx, kGAB_NUMBER);
  }

  size_t i = gab_valton(idx);

  if (i >= gab_reclen(rec)) {
    gab_vmpush(gab.vm, gab_none);
    return nullptr;
  }

  gab_value results[3] = {
      gab_ok,
      gab_ushpat(gab_recshp(rec), i),
      gab_urecat(rec, i),
  };

  gab_nvmpush(gab.vm, 3, results);
  return nullptr;
}

a_gab_value *gab_lib_atn(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  for (size_t i = 1; i < argc; i++) {
    gab_value res = gab_recat(rec, argv[i]);

    if (res == gab_undefined)
      return gab_panic(gab, "$ has no property $", rec, argv[i]);

    gab_vmpush(gab.vm, res);
  }

  return nullptr;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value r = gab_arg(0);
  gab_value k = gab_arg(1);
  gab_value v = gab_arg(2);

  if (gab_recput(gab, r, k, v) == gab_undefined)
    return gab_panic(gab, "Failed to put $ in $", k, r);
  else
    gab_vmpush(gab.vm, r);

  return nullptr;
}

a_gab_value *gab_lib_push(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc != 2)
    return gab_panic(gab, "Invalid call to gab_lib_push");

  gab_value tup =
      gab_recordwith(gab, argv[0], gab_number(gab_reclen(argv[0])), argv[1]);

  gab_vmpush(gab.vm, tup);

  return nullptr;
}

a_gab_value *gab_lib_clear(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  if (argc < 1)
    return gab_panic(gab, "Invalid call to gab_lib_clear");

  gab_value rec = gab_arg(0);

  for (size_t i = 0; i < gab_reclen(rec); i++) {
    gab_recput(gab, rec, i, gab_nil);
  }

  return nullptr;
}

a_gab_value *gab_lib_with(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {

  gab_value key = gab_arg(1);
  gab_value val = gab_arg(2);

  gab_value rec = gab_recordwith(gab, argv[0], key, val);

  gab_vmpush(gab.vm, rec);

  return nullptr;
}

a_gab_value *gab_lib_tuple(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to :tuple");
    }

    gab_value result = gab_etuple(gab, gab_valton(argv[1]));

    gab_vmpush(gab.vm, result);

    return nullptr;
  }
  default:
    return gab_panic(gab, "Invalid call to :tuple");
  }
}

a_gab_value *gab_lib_shape(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_SHAPE) {
      return gab_pktypemismatch(gab, argv[1], kGAB_SHAPE);
    }

    gab_value result = gab_erecordof(gab, argv[1]);

    gab_vmpush(gab.vm, result);
    return nullptr;
  }
  default:
    return gab_panic(gab, "Expected 1 argument to :record.new");
  }
};

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value result = gab_number(gab_reclen(argv[0]));

    gab_vmpush(gab.vm, result);
    return nullptr;
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_len");
  }
}

a_gab_value *gab_lib_to_l(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list =
        list_create(gab, gab_reclen(argv[0]), gab_recdata(argv[0]));

    gab_vmpush(gab.vm, list);
    return nullptr;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_l");
  }
}

a_gab_value *gab_lib_compact(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  gab_value rec = argv[0];
  gab_value shp = gab_recshp(rec);

  switch (argc) {
  case 1: {
    size_t keeplen = 0, len = gab_reclen(rec);
    gab_value keepkeys[len] = {};
    gab_value keepvalues[len] = {};
    for (size_t i = 0; i < len; i++) {
      if (gab_valintob(gab_urecat(rec, i))) {
        keepkeys[keeplen] = gab_ushpat(shp, i);
        keepvalues[keeplen] = gab_urecat(rec, i);
        keeplen++;
      }
    }

    gab_value result = gab_record(gab, keeplen, keepkeys, keepvalues);

    gab_vmpush(gab.vm, result);

    return nullptr;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_m");
  }
}

a_gab_value *gab_lib_to_m(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value rec = argv[0];
  gab_value shp = gab_recshp(rec);

  switch (argc) {
  case 1: {
    gab_value map =
        map_create(gab, gab_reclen(rec), 1, gab_shpdata(shp), gab_recdata(rec));

    gab_vmpush(gab.vm, map);
    return nullptr;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_m");
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_type(gab.eg, kGAB_RECORD);
  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_sigil(gab, "gab.tuple"),
          gab_snative(gab, "tuple", gab_lib_tuple),
      },
      {
          mGAB_CALL,
          gab_sigil(gab, "gab.record"),
          gab_snative(gab, "record", gab_lib_shape),
      },
      {
          "len",
          type,
          gab_snative(gab, "len", gab_lib_len),
      },
      {
          "put!",
          type,
          gab_snative(gab, "put", gab_lib_put),
      },
      {
          "at",
          type,
          gab_snative(gab, "at", gab_lib_at),
      },
      {
          "at_index",
          type,
          gab_snative(gab, "at_index", gab_lib_at_index),
      },
      {
          "at!",
          type,
          gab_snative(gab, "at!", gab_lib_atn),
      },
      {
          "slice",
          type,
          gab_snative(gab, "slice", gab_lib_slice),
      },
      {
          "push",
          type,
          gab_snative(gab, "push", gab_lib_push),
      },
      {
          "with",
          type,
          gab_snative(gab, "with", gab_lib_with),
      },
      {
          "clear!",
          type,
          gab_snative(gab, "clear", gab_lib_clear),
      },
      {
          "compact",
          type,
          gab_snative(gab, "compact", gab_lib_compact),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return a_gab_value_one(type);
}
