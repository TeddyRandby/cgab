#include "include/object.h"
#include "include/gab.h"
#include "include/core.h"

gab_value gab_lib_keys(gab_engine *gab, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

  u64 size = obj->is_dynamic ? obj->dynamic_values.len : obj->static_size;

  gab_obj_shape *shape = gab_obj_shape_create_array(gab, size);

  gab_value list =
      GAB_VAL_OBJ(gab_obj_record_create(shape, obj->shape->keys, size, 1));

  return list;
}

gab_value gab_lib_len(gab_engine *gab, gab_value *argv, u8 argc) {
  if (GAB_VAL_IS_RECORD(argv[0])) {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

    return GAB_VAL_NUMBER(obj->is_dynamic ? obj->dynamic_values.len
                                          : obj->static_size);
  }

  if (GAB_VAL_IS_STRING(argv[0])) {
    gab_obj_string *obj = GAB_VAL_TO_STRING(argv[0]);

    return GAB_VAL_NUMBER(obj->len);
  }

  return GAB_VAL_NULL();
}

gab_value gab_lib_push(gab_engine *gab, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    return GAB_VAL_NULL();
  }

  if (argc < 2) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

  for (u8 i = 1; i < argc; i++) {
    u64 index = obj->is_dynamic ? obj->dynamic_values.len : obj->static_size;
    gab_obj_record_insert(gab, obj, GAB_VAL_NUMBER(index), argv[i]);
  }

  return GAB_VAL_OBJ(obj);
}

gab_value gab_mod(gab_engine *gab) {
  s_i8 keys[] = {
      s_i8_cstr("keys"),
      s_i8_cstr("len"),
      s_i8_cstr("push"),
  };

  gab_value values[] = {
      GAB_BUILTIN(keys),
      GAB_BUILTIN(len),
      GAB_BUILTIN(push),
  };

  return gab_bundle_record(gab, LEN_CARRAY(values), keys, values);
}
