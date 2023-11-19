#include "gab.h"

void gab_lib_message(struct gab_triple gab, size_t argc,
                     gab_value argv[static argc]) {
  if (argc != 2 || gab_valkind(argv[1]) != kGAB_STRING) {
    gab_panic(gab, "INVALID_ARGUMENTS");
    return;
  }

  gab_vmpush(gab.vm, gab_message(gab, argv[1]));
}

void gab_lib_put(struct gab_triple gab, size_t argc,
                 gab_value argv[static argc]) {
  gab_value rec = gab_undefined;
  switch (argc) {
  case 3:
    rec = argv[1];
    [[fallthrough]];
  case 2: {
    gab_value result = gab_msgput(gab, argv[0], rec, argv[argc - 1]);

    if (result == gab_undefined) {
      gab_vmpush(gab.vm, gab_string(gab, "SPECIALIZATION_EXISTS"));
      return;
    }

    gab_vmpush(gab.vm, gab_string(gab, "ok"));
    gab_vmpush(gab.vm, result);
    return;
  }

  default:
    gab_panic(gab, "INVALID_ARGUMENTS");
    return;
  }
}

void gab_lib_at(struct gab_triple gab, size_t argc,
                gab_value argv[static argc]) {
  switch (argc) {
  case 2: {
    gab_value result = gab_msgat(argv[0], argv[1]);
    
    if (result == gab_undefined) {
      gab_vmpush(gab.vm, gab_string(gab, "none"));
      return;
    }
    
    gab_vmpush(gab.vm, gab_string(gab, "some"));
    gab_vmpush(gab.vm, result);
    return;
  }
  default:
    gab_panic(gab, "INVALID_ARGUMENTS");
    return;
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "message.new",
          gab_undefined,
          gab_snative(gab, "message.new", gab_lib_message),
      },
      {
          "put!",
          gab_type(gab.eg, kGAB_MESSAGE),
          gab_snative(gab, "put!", gab_lib_put),
      },
      {
          "at",
          gab_type(gab.eg, kGAB_MESSAGE),
          gab_snative(gab, "at", gab_lib_at),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
