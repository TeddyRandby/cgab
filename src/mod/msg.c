#include "gab.h"

a_gab_value* gab_lib_message(struct gab_triple gab, size_t argc,
                    gab_value argv[static argc]) {
  if (argc != 2 || gab_valkind(argv[1]) != kGAB_STRING) {
    return gab_panic(gab, "INVALID_ARGUMENTS");
  }

  gab_vmpush(gab.vm, gab_message(gab, argv[1]));
  return NULL;
}

a_gab_value* gab_lib_at(struct gab_triple gab, size_t argc,
                gab_value argv[static argc]) {
  switch (argc) {
  case 2: {

    struct gab_egimpl_rest res = gab_egimpl(gab.eg, argv[0], argv[1]);

    gab_vmpush(gab.vm, gab_bool(res.status));
    return NULL;
  }
  default:
    return gab_panic(gab, "INVALID_ARGUMENTS");
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
          "has?",
          gab_type(gab.eg, kGAB_MESSAGE),
          gab_snative(gab, "has?", gab_lib_at),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
