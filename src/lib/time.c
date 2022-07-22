#include "lib_time.h"
#include <time.h>
#include <unistd.h>

gab_value gab_lib_time_clock(u8 argc, gab_value *argv, gab_engine *eng,
                             char **err) {
  if (argc != 0) {
    *err = "Expected zero arguments";
    return GAB_VAL_NULL();
  }

  clock_t t = clock();

  return GAB_VAL_NUMBER((f64)t / CLOCKS_PER_SEC);
};

gab_value gab_lib_time_sleep(u8 argc, gab_value *argv, gab_engine *eng,
                             char **err) {
  if (argc != 1) {
    *err = "Expected one argument";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[0])) {
    *err = "Expected a number";
    return GAB_VAL_NULL();
  }

  sleep(GAB_VAL_TO_NUMBER(argv[0]));

  return GAB_VAL_NULL();
}
