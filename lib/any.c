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

void gab_lib_specializes(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, "INVALID_ARGUMENTS");
    return;
  }
  
  size_t offset = gab_msgfind(argv[0], argv[1]);

  if (offset == GAB_PROPERTY_NOT_FOUND) {
    gab_vmpush(gab.vm, gab_string(gab, "none"));
    return;
  }
  
  gab_vmpush(gab.vm, gab_string(gab, "some"));
  gab_vmpush(gab.vm, gab_umsgat(argv[0], offset));
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "specializes?",
          gab_type(gab.eg, kGAB_MESSAGE),
          gab_snative(gab, "specializes?", gab_lib_specializes),
      },
      {
          "send",
          gab_type(gab.eg, kGAB_UNDEFINED),
          gab_snative(gab, "send", gab_lib_send),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
