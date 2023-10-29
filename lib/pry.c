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

void gab_lib_pryself(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  size_t depth = 0;
  
  if (argc == 1 && gab_valknd(argv[0]) == kGAB_NUMBER) {
    depth = gab_valton(argv[0]);
  }

  gab_fpry(stdout, gab.vm, depth);
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value recs[] = {
      gab_string(gab, "gab_vm"),
      gab_nil,
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "pry", gab_lib_pryframes),
      gab_sbuiltin(gab, "pry", gab_lib_pryself),
  };

  const char *names[] = {
      "pry",
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

  return a_gab_value_one(gab_nil);
}
