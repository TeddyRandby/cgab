#include "print.h"
#include <stdio.h>

gab_value gab_lib_print(gab_engine *eng, gab_vm *vm, gab_value receiver,
                        u8 argc, gab_value argv[argc]) {
  gab_val_dump(receiver);

  for (u8 i = 0; i < argc; i++) {
      putc(' ', stdout);
      gab_val_dump(argv[i]);
  }

  printf("\n");

  return GAB_VAL_NULL();
}
