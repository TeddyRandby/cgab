#include "../gab/gab.h"
#include <string.h>

gab_value gab_lib_tonum(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_NUMBER(
      strtod((const char *)GAB_VAL_TO_STRING(argv[0])->data, NULL));
};

gab_value gab_lib_slice(gab_engine *eng, gab_value *argv, u8 argc) {
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

  if (offset + size > src->size) {
    return GAB_VAL_NULL();
  }

  s_i8 result = s_i8_create(src->data + offset, size);

  return GAB_VAL_OBJ(gab_obj_string_create(eng, result));
};

gab_value gab_mod(gab_engine *gab) {
  gab_lib_kvp str_kvps[] = {GAB_KVP_BUILTIN(tonum, 1), GAB_KVP_BUILTIN(slice, 3)};
  return gab_kvp_bundle(gab, GAB_KVP_BUNDLESIZE(str_kvps), str_kvps);
}
