#include "symbol.h"

void gab_lib_new(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    gab_value name = argv[1];

    gab_value sym = symbol_create(gab, vm, name);

    gab_vmpush(vm, sym);

    return;
  }
  default:
    gab_panic(gab, vm, "&:new expects 2 arguments");
    return;
  }
}

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "symbol",
  };

  gab_value receivers[] = {
      gab_nil,
  };

  gab_value specs[] = {
      gab_builtin(gab, "new", gab_lib_new),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = receivers[i],
                 .specialization = specs[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(specs), specs);

  return NULL;
}
