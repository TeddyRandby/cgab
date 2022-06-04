#include "lib_log.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

gab_value gab_lib_log_error(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err) {

  printf(ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET ": ");

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

  return GAB_VAL_NULL();
}

gab_value gab_lib_log_warn(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err) {

  printf(ANSI_COLOR_YELLOW "WARNING" ANSI_COLOR_RESET ": ");

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

  return GAB_VAL_NULL();
}

gab_value gab_lib_log_info(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err) {
  printf(ANSI_COLOR_BLUE "INFO" ANSI_COLOR_RESET ": ");

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

  return GAB_VAL_NULL();
}

gab_value gab_lib_log_print(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err) {
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

  return GAB_VAL_NULL();
}
