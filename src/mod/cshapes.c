#include "gab.h"
#include "list.h"
#include "map.h"

a_gab_value *gab_lib_splat(struct gab_triple gab, size_t argc,
                           gab_value argv[static argc]) {
  gab_nvmpush(gab.vm, gab_shplen(argv[0]), gab_shpdata(argv[0]));
  return nullptr;
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))
a_gab_value *gab_lib_slice(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  gab_value shp = argv[0];

  uint64_t len = gab_shplen(shp);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 1:
    break;

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

  case 2:
    if (gab_valkind(argv[1]) == kGAB_NUMBER) {
      uint64_t a = gab_valton(argv[1]);
      end = MIN(a, len);
      break;
    }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_slice");
  }

  uint64_t result_len = end - start;

  gab_value result = gab_shape(gab, 1, result_len, gab_shpdata(shp) + start);

  gab_vmpush(gab.vm, result);
  return nullptr;
}
#undef MIN
#undef MAX
#undef CLAMP

a_gab_value *gab_lib_next(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value shp = argv[0];

  gab_value res = gab_nil;

  size_t len = gab_shplen(shp);

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
  return nullptr;
}

a_gab_value *gab_lib_shape(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  gab_value result = gab_shape(gab, 1, argc - 1, argv + 1);

  gab_vmpush(gab.vm, result);
  return nullptr;
};

a_gab_value *gab_lib_with(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value key = gab_arg(1);

  gab_value rec = gab_shapewith(gab, argv[0], key);

  gab_vmpush(gab.vm, rec);

  return nullptr;
}

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value result = gab_number(gab_shplen(argv[0]));

  gab_vmpush(gab.vm, result);
  return nullptr;
}

a_gab_value *gab_lib_to_l(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value list = list_create(gab, gab_shplen(argv[0]), gab_shpdata(argv[0]));

  gab_vmpush(gab.vm, list);
  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "shape.new",
          gab_undefined,
          gab_snative(gab, "shape.new", gab_lib_shape),
      },
      {
          "len",
          gab_egtype(gab.eg, kGAB_SHAPE),
          gab_snative(gab, "len", gab_lib_len),
      },
      {
          "to_l",
          gab_egtype(gab.eg, kGAB_SHAPE),
          gab_snative(gab, "to_l", gab_lib_to_l),
      },
      {
          "next",
          gab_egtype(gab.eg, kGAB_SHAPE),
          gab_snative(gab, "next", gab_lib_next),
      },
      {
          "slice",
          gab_egtype(gab.eg, kGAB_SHAPE),
          gab_snative(gab, "slice", gab_lib_slice),
      },
      {
          "splat",
          gab_egtype(gab.eg, kGAB_SHAPE),
          gab_snative(gab, "splat", gab_lib_splat),
      },
      {
          "with",
          gab_egtype(gab.eg, kGAB_SHAPE),
          gab_snative(gab, "with", gab_lib_with),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return nullptr;
}
