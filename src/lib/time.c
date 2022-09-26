#include "../gab/gab.h"
#include "src/core/core.h"
#include "src/gab/object.h"
#include <time.h>
#include <unistd.h>

gab_value gab_lib_clock(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 0) {
    return GAB_VAL_NULL();
  }

  clock_t t = clock();

  return GAB_VAL_NUMBER((f64)t / CLOCKS_PER_SEC);
};

gab_value gab_lib_sleep(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 1) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  sleep(GAB_VAL_TO_NUMBER(argv[0]));

  return GAB_VAL_NULL();
}

gab_value gab_mod(gab_engine *gab) {
  s_i8 keys[] = {
      s_i8_cstr("clock"),
      s_i8_cstr("sleep"),
  };

  gab_value values[] = {
      GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_clock, "clock", 0)),
      GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_sleep, "sleep", 1)),
  };

  return gab_bundle(gab, sizeof(values) / sizeof(gab_value), keys, values);
}
