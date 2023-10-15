#include "include/gab.h"
#include <assert.h>
#include <time.h>

void gab_lib_now(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_clock");
    return;
  }

  clock_t t = clock();

  gab_value res = gab_number((double)t / CLOCKS_PER_SEC);

  gab_vmpush(vm, res);
};

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  const char *names[] = {
      "now",
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "now", gab_lib_now),
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

  return NULL;
}
