#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include <assert.h>
#include <time.h>

void gab_lib_clock(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_clock");
    return;
  }

  clock_t t = clock();

  gab_value res = GAB_VAL_NUMBER((f64)t / CLOCKS_PER_SEC);

  gab_push(vm, 1, &res);
};

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value keys[] = {
      GAB_STRING("clock"),
  };

  gab_value values[] = {
      GAB_BUILTIN(clock),
  };

  static_assert(LEN_CARRAY(keys) == LEN_CARRAY(values));

  for (int i = 0; i < LEN_CARRAY(keys); i++) {
    gab_specialize(gab, vm, keys[i], GAB_VAL_NIL(), values[i]);
    gab_val_dref(vm, values[i]);
  }

  return GAB_VAL_NIL();
}
