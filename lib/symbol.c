#include "symbol.h"
#include <curses.h>

void gab_lib_new(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    gab_value name = argv[1];

    gab_value sym = symbol_create(gab, name);

    gab_vmpush(gab.vm, sym);

    return;
  }
  default:
    gab_panic(gab, "&:new expects 2 arguments");
    return;
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  const char *names[] = {
      "symbol",
  };

  gab_value receivers[] = {
      gab_nil,
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "new", gab_lib_new),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = specs[i],
                  });
  }

  return NULL;
}
