#include "builtins.h"
#include <stdio.h>

gab_value gab_lib_print(gab_engine *gab, u8 argc, gab_value argv[argc]) {
  for (u8 i = 0; i < argc; i++) {
    if (i > 0)
      putc(' ', stdout);
    gab_val_dump(stdout, argv[i]);
  }

  printf("\n");

  return GAB_VAL_NIL();
}
