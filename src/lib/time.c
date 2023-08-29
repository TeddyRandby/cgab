#include "include/gab.h"
#include <assert.h>
#include <time.h>

void gab_lib_now(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_clock");
    return;
  }

  clock_t t = clock();

  gab_value res = gab_number((f64)t / CLOCKS_PER_SEC);

  gab_vmpush(vm, res);
};

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *keys[] = {
      "now",
  };

  gab_value values[] = {
      gab_builtin(gab, "now", gab_lib_now),
  };

  gab_value receiver = gab_nil;

  static_assert(LEN_CARRAY(keys) == LEN_CARRAY(values));

  for (int i = 0; i < LEN_CARRAY(keys); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .receiver = receiver,
                 .name = keys[i],
                 .specialization = values[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(values), values);

  return NULL;
}
