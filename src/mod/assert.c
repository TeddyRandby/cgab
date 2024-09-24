#include "gab.h"

a_gab_value *gab_lib_assertis(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  gab_value r = gab_arg(0);
  gab_value t = gab_arg(1);

  if (gab_valtype(gab, r) != t)
    return gab_ptypemismatch(gab, r, t);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {

  struct gab_def_argt specs[] = {
      {
          gab_message(gab, "assert.is?"),
          gab_undefined,
          gab_snative(gab, "assert.is?", gab_lib_assertis),
      },
  };

  gab_ndef(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return nullptr;
}
