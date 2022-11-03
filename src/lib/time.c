#include "include/gab.h"
#include "include/core.h"
#include "include/object.h"
#include <time.h>
#include <unistd.h>

gab_value gab_lib_clock(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]) {
  if (argc != 0) {
    return GAB_VAL_NULL();
  }

  clock_t t = clock();

  return GAB_VAL_NUMBER((f64)t / CLOCKS_PER_SEC);
};

gab_value gab_lib_sleep(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  sleep(GAB_VAL_TO_NUMBER(argv[0]));

  return GAB_VAL_NULL();
}

gab_value gab_mod(gab_engine *gab, gab_vm* vm) {
  s_i8 keys[] = {
      s_i8_cstr("clock"),
      s_i8_cstr("sleep"),
  };

  gab_value values[] = {
      GAB_BUILTIN(clock),
      GAB_BUILTIN(sleep),
  };

  return gab_bundle_record(gab, vm, LEN_CARRAY(values), keys, values);
}
