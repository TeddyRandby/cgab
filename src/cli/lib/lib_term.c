#include "lib_term.h"

gab_value gab_lib_term_clear(u8 argc, gab_value *argv, gab_engine *eng,
                             char **err) {
  if (argc != 0) {
    *err = "Expected zero arguments";
    return GAB_VAL_NULL();
  }
  printf("\033[0;0H\033[2J");
  return GAB_VAL_NULL();
};
