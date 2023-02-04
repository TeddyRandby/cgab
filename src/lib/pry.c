#include "../include/gab.h"
#include "include/core.h"
#include "include/object.h"
#include "include/vm.h"

gab_value gab_lib_pryframes(gab_engine *gab, gab_vm *vm, u8 argc,
                            gab_value argv[argc]) {
    if (argc < 1 || !GAB_VAL_IS_CONTAINER(argv[0])) {
        gab_panic(gab, vm, "Invalid call to gab_lib_pryframes");
    }

    gab_obj_container* con = GAB_VAL_TO_CONTAINER(argv[0]);

    u64 depth = 0;

    gab_vm_frame_dump(gab, con->data, depth);

    return argv[0];
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value receivers[] = {
      gab_get_type(gab, TYPE_CONTAINER),
  };

  gab_value values[] = {
      GAB_BUILTIN(pryframes),
  };

  s_i8 names[] = {
      s_i8_cstr("frame"),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (u8 i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, names[i], receivers[i], values[i]);
    gab_dref(gab, vm, values[i]);
  }

  return GAB_VAL_NIL();
}
