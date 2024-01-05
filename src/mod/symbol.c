#include "symbol.h"
#include <curses.h>

a_gab_value* gab_lib_new(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    gab_value name = argv[1];

    gab_value sym = symbol_create(gab, name);

    gab_vmpush(gab.vm, sym);

    return NULL;
  }
  default:
    return gab_panic(gab, "&:symbol.new expects 2 arguments");
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "symbol.new",
          gab_undefined,
          gab_snative(gab, "new", gab_lib_new),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
