#include "builtins.h"
#include "include/object.h"
#include <stdlib.h>
gab_value gab_lib_panic(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc == 1) {
    gab_obj_string *str = GAB_VAL_TO_STRING(gab_val_to_string(gab, argv[0]));
    char buffer[str->len + 1];
    memcpy(buffer, str->data, str->len);
    buffer[str->len] = '\0';
    gab_panic(gab, vm, buffer);
  } else {
    gab_panic(gab, vm, "");
  }
}
