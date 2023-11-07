#include "include/core.h"
#include "include/gab.h"
#include <stdint.h>

void gab_lib_send(struct gab_triple gab, size_t argc,
                  gab_value argv[static argc]) {
  if (argc < 2) {
    gab_panic(gab, "Invalid call to gab_lib_send");
    return;
  }

  a_gab_value *result = gab_send(gab, (struct gab_send_argt){
                                          .flags = 0,
                                          .receiver = argv[0],
                                          .vmessage = argv[1],
                                          .len = argc - 2,
                                          .argv = argv + 2,
                                      });

  if (!result) {
    gab_panic(gab, "Invalid send");
    return;
  }

  gab_nvmpush(gab.vm, result->len, result->data);

  a_gab_value_destroy(result);
}

void gab_lib_is_n(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  gab_vmpush(gab.vm, gab_bool(gab_valkind(argv[0]) == kGAB_NUMBER));
}

void gab_lib_is_s(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  gab_vmpush(gab.vm, gab_bool(gab_valkind(argv[0]) == kGAB_STRING));
}

void gab_lib_is_b(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  gab_vmpush(gab.vm, gab_bool(gab_valkind(argv[0]) == kGAB_FALSE ||
                              gab_valkind(argv[0]) == kGAB_TRUE));
}

void gab_lib_implements(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "implements?",
          gab_type(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "implements?", gab_lib_implements),
      },
      {
          "send",
          gab_type(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "send", gab_lib_send),
      },
      {
          "is_n",
          gab_type(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "is_n", gab_lib_is_n),
      },
      {
          "is_s",
          gab_type(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "is_s", gab_lib_is_s),
      },
      {
          "is_b",
          gab_type(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "is_b", gab_lib_is_b),
      },

  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
