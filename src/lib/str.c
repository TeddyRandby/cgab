#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include <string.h>

gab_value gab_lib_slice(gab_engine *eng, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[1])) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[2])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *src = GAB_VAL_TO_STRING(argv[0]);
  f64 offsetf = GAB_VAL_TO_NUMBER(argv[1]);
  f64 sizef = GAB_VAL_TO_NUMBER(argv[2]);
  u64 offset = offsetf;
  u64 size = sizef;

  if (offset + size > src->len) {
    return GAB_VAL_NULL();
  }

  s_i8 result = s_i8_create(src->data + offset, size);

  return GAB_VAL_OBJ(gab_obj_string_create(eng, result));
};

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  s_i8 keys[] = {
      s_i8_cstr("slice"),
  };

  gab_value values[] = {
      GAB_BUILTIN(slice),
  };

  return gab_bundle_record(gab, vm, LEN_CARRAY(values), keys, values);
}
