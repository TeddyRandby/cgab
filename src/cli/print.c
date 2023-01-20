#include "print.h"
#include <stdio.h>

gab_value gab_lib_print(gab_engine *eng, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  for (u8 i = 0; i < argc; i++) {
    putc(' ', stdout);
    gab_val_dump(argv[i]);
  }

  printf("\n");

  return GAB_VAL_NULL();
}
