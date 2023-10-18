#include "../include/gab.h"
#include <stdio.h>

void gab_lib_pryframes(struct gab_triple gab, size_t argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, "Invalid call to gab_lib_pryframes");
  }

  if (argc == 1) {
    gab_fpry(stdout, gab.vm, 0);

    return;
  }

  if (argc == 2 && gab_valknd(argv[1]) == kGAB_NUMBER) {
    uint64_t depth = gab_valton(argv[1]);

    gab_fpry(stdout, gab.vm, depth);

    return;
  }

  return;
}

gab_value gab_lib(struct gab_triple gab) {
  gab_value recs[] = {
      gab_string(gab.eg, "gab_vm"),
  };

  gab_value specs[] = {
      gab_sbuiltin(gab.eg, "pry", gab_lib_pryframes),
  };

  const char *names[] = {
      "pry",
  };

  static_assert(LEN_CARRAY(specs) == LEN_CARRAY(recs));
  static_assert(LEN_CARRAY(specs) == LEN_CARRAY(names));

  for (uint8_t i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = recs[i],
                      .specialization = specs[i],
                  });
  }

  return gab_nil;
}
