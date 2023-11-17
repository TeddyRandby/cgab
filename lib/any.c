#include "include/core.h"
#include "include/gab.h"
#include <stdint.h>

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
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
