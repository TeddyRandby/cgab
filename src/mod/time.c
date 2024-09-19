#include "gab.h"
#include <time.h>

a_gab_value* gab_lib_now(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    return gab_fpanic(gab, "Invalid call to gab_lib_clock");
  }

  clock_t t = clock();

  gab_value res = gab_number((double)t / CLOCKS_PER_SEC);

  gab_vmpush(gab_vm(gab), res);
  return nullptr;
};

a_gab_value *gab_lib(struct gab_triple gab) {
  const char *names[] = {
      "now",
  };

  gab_value specs[] = {
      gab_snative(gab, "now", gab_lib_now),
  };

  gab_value receivers[] = {
      gab_nil,
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(names); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = specs[i],
                  });
  }

  return nullptr;
}
