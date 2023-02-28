#include "../include/gab.h"
#include "include/core.h"
#include "include/object.h"
#include "include/value.h"
#include "include/vm.h"

gab_value gab_lib_pryframes(gab_engine *gab, u8 argc, gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, "Invalid call to gab_lib_pryframes");
  }

  if (argc == 1) {
    gab_vm_frame_dump(gab, 0);

    return argv[0];
  }

  if (argc == 2 && GAB_VAL_IS_NUMBER(argv[1])) {
    u64 depth = GAB_VAL_TO_NUMBER(argv[1]);

    gab_vm_frame_dump(gab, depth);

    return argv[0];
  }

  return GAB_VAL_NIL();
}

gab_value gab_mod(gab_engine *gab) {
  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_CONTAINER),
  };

  gab_value values[] = {
      GAB_BUILTIN(pryframes),
  };

  gab_value names[] = {
      GAB_STRING("frame"),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (u8 i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, names[i], receivers[i], values[i]);
    gab_dref(gab, values[i]);
  }

  return GAB_VAL_NIL();
}
