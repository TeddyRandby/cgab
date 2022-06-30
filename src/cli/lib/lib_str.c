#include "lib.h"
#include <string.h>

gab_value gab_lib_num(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_NUMBER(
      strtod((const char *)GAB_VAL_TO_STRING(argv[0])->data, NULL));
};
