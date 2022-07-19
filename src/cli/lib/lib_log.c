#include "lib.h"
#include <stdio.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

static inline void print_values(gab_value *argv, u8 argc) {
  for (i32 i = 0; i < argc; i++) {
    gab_value arg = argv[i];
    gab_val_dump(arg);

    if (i == (argc - 1)) {
      // last iteration
      printf("\n");
    } else {
      printf(", ");
    }
  }
}

gab_value gab_lib_error(gab_engine *eng, gab_value *argv, u8 argc) {
  printf(ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET ": ");

  print_values(argv, argc);

  return GAB_VAL_NULL();
}

gab_value gab_lib_warn(gab_engine *eng, gab_value *argv, u8 argc) {
  printf(ANSI_COLOR_YELLOW "WARNING" ANSI_COLOR_RESET ": ");

  print_values(argv, argc);
  return GAB_VAL_NULL();
}

gab_value gab_lib_info(gab_engine *eng, gab_value *argv, u8 argc) {
  print_values(argv, argc);

  return GAB_VAL_NULL();
}
