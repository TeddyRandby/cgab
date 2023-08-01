#include "symbol.h"

void gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    gab_value name = argv[1];

    gab_value sym = GAB_VAL_OBJ(symbol_create(gab, vm, name));

    gab_push(vm, sym);

    return;
  }
  default:
    gab_panic(gab, vm, "&:new expects 2 arguments");
    return;
  }
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("symbol"),
  };

  gab_value receivers[] = {
      GAB_VAL_NIL(),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
