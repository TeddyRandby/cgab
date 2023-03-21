#include "builtins.h"
#include <stdio.h>

void gab_lib_print(gab_engine *gab, gab_vm* vm, u8 argc, gab_value argv[argc]) {
  for (u8 i = 0; i < argc; i++) {
    if (i > 0)
      putc(' ', stdout);
    gab_val_dump(stdout, argv[i]);
  }

  printf("\n");
}
