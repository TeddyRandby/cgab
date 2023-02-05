#include "builtins.h"
#include "include/gab.h"

gab_value gab_lib_type(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  return gab_val_type(gab, argv[0]);
}
