#include "print.h"
#include <stdio.h>

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

gab_value gab_lib_print(gab_engine *eng, i32 vm, u8 argc, gab_value argv[argc]) {
  print_values(argv, argc);
  return GAB_VAL_NULL();
}
