#include "../gab/gab.h"
#include <time.h>
#include <unistd.h>

gab_value gab_lib_clock(gab_engine *eng, gab_value *argv,u8 argc) {
  if (argc != 0) {
    return GAB_VAL_NULL();
  }

  clock_t t = clock();

  return GAB_VAL_NUMBER((f64)t / CLOCKS_PER_SEC);
};

gab_value gab_lib_sleep(gab_engine *eng, gab_value *argv,u8 argc) {
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
  gab_lib_kvp time_kvps[] = {GAB_KVP_BUILTIN(clock, 0), GAB_KVP_BUILTIN(sleep, 1)};
  return gab_bundle_kvps(gab, GAB_KVP_BUNDLESIZE(time_kvps), time_kvps);
}
