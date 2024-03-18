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

  gab_value result = gab_tuple(gab, result_len, gab_recdata(rec) + start);

  gab_vmpush(gab.vm, result);

  return NULL;
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
    gab_vmpush(gab.vm, gab_string(gab, "none"));
    return NULL;
  }

  gab_vmpush(gab.vm, gab_string(gab, "some"));
  gab_vmpush(gab.vm, result);
  return NULL;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value r = gab_arg(0);
  gab_value k = gab_arg(1);
  gab_value v = gab_arg(2);

  if (gab_recput(gab, r, k, v) == gab_undefined)
    gab_vmpush(gab.vm, gab_nil);
  else
    gab_vmpush(gab.vm, v);

  return NULL;
}

a_gab_value *gab_lib_push(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc != 2)
    return gab_panic(gab, "Invalid call to gab_lib_push");

  gab_value tup =
      gab_recordwith(gab, argv[0], gab_number(gab_reclen(argv[0])), argv[1]);

  gab_vmpush(gab.vm, tup);

  return NULL;
}

a_gab_value *gab_lib_clear(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc < 1)
    return gab_panic(gab, "Invalid call to gab_lib_push");

  gab_value rec = gab_arg(0);

  for (size_t i = 0; i < gab_reclen(rec); i++) {
    gab_recput(gab, rec, i, gab_nil);
  }

  return NULL;
}

a_gab_value *gab_lib_with(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc < 3)
    return gab_panic(gab, "Invalid call to gab_lib_push");

  gab_value key = argv[1];
  gab_value val = argc > 2 ? argv[2] : gab_nil;

  gab_value rec = gab_recordwith(gab, argv[0], key, val);

  gab_vmpush(gab.vm, rec);

  return NULL;
}

a_gab_value *gab_lib_next(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value rec = argv[0];
  gab_value shp = gab_recshp(rec);

  gab_value res = gab_nil;

  size_t len = gab_reclen(rec);

  if (len < 1)
    goto fin;

  switch (argc) {
  case 1:
    res = gab_shpdata(shp)[0];
    goto fin;
  case 2: {
    size_t current = gab_shpfind(shp, argv[1]);

    if (current == UINT64_MAX || current + 1 == len)
      goto fin;

    res = gab_shpdata(shp)[current + 1];
    goto fin;
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_next");
  }

fin:
  gab_vmpush(gab.vm, res);
  return NULL;
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

    return NULL;
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
      return gab_ptypemismatch(gab, argv[1], gab_type(gab.eg, kGAB_SHAPE));
    }

    gab_value result = gab_erecordof(gab, argv[1]);

    gab_vmpush(gab.vm, result);
    return NULL;
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
    return NULL;
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
    return NULL;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_l");
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
    return NULL;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_m");
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "make",
          gab_sigil(gab, "gab.tuple"),
          gab_snative(gab, "tuple.new", gab_lib_tuple),
      },
      {
          "make",
          gab_sigil(gab, "gab.record"),
          gab_snative(gab, "record.new", gab_lib_shape),
      },
      {
          "len",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "len", gab_lib_len),
      },
      {
          "to_l",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "to_l", gab_lib_to_l),
      },
      {
          "to_m",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "to_m", gab_lib_to_m),
      },
      {
          "put!",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "put", gab_lib_put),
      },
      {
          "at",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "at", gab_lib_at),
      },
      {
          "next",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "next", gab_lib_next),
      },
      {
          "slice",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "slice", gab_lib_slice),
      },
      {
          "push",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "push", gab_lib_push),
      },
      {
          "with",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "with", gab_lib_with),
      },
      {
          "clear!",
          gab_type(gab.eg, kGAB_RECORD),
          gab_snative(gab, "clear", gab_lib_clear),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
