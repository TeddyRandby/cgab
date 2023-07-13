#include "../include/gab.h"

void gab_lib_pryframes(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_pryframes");
  }

  if (argc == 1) {
    gab_pry(vm, 0);

    return;
  }

  if (argc == 2 && GAB_VAL_IS_NUMBER(argv[1])) {
    u64 depth = GAB_VAL_TO_NUMBER(argv[1]);

    gab_pry(vm, depth);

    return;
  }

  return;
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value receivers[] = {
      GAB_STRING("gab_vm"),
  };

  gab_value values[] = {
      GAB_BUILTIN(pryframes),
  };

  gab_value names[] = {
      GAB_STRING("pry"),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (u8 i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  return GAB_VAL_NIL();
}
