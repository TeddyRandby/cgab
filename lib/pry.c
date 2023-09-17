#include "../include/gab.h"
#include <stdio.h>

void gab_lib_pryframes(gab_eg *gab, gab_vm *vm, size_t argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_pryframes");
  }

  if (argc == 1) {
    gab_fpry(stdout, vm, 0);

    return;
  }

  if (argc == 2 && gab_valknd(argv[1]) == kGAB_NUMBER) {
    uint64_t depth = gab_valton(argv[1]);

    gab_fpry(stdout, vm, depth);

    return;
  }

  return;
}

gab_value gab_lib(gab_eg *gab, gab_vm *vm) {
  gab_value receivers[] = {
      gab_string(gab,"gab_vm"),
  };

  gab_value values[] = {
      gab_builtin(gab, "pry", gab_lib_pryframes),
  };

  gab_value names[] = {
      gab_string(gab,"pry"),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (uint8_t i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  return gab_nil;
}
