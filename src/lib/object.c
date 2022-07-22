#include "lib.h"

gab_value gab_lib_keys(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_OBJECT(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_object *obj = GAB_VAL_TO_OBJECT(argv[0]);

  u64 size = obj->is_dynamic ? obj->dynamic_values.size : obj->static_size;

  gab_obj_shape *shape = gab_obj_shape_create_array(eng, size);

  gab_obj_object *list =
      gab_obj_object_create(eng, shape, obj->shape->keys, size, 1);

  gab_engine_obj_dref(eng, (gab_obj *)list);

  return GAB_VAL_OBJ(list);
}

gab_value gab_lib_slice(gab_engine *eng, gab_value *argv, u8 argc) {

  if (!GAB_VAL_IS_NUMBER(argv[1])) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[2])) {
    return GAB_VAL_NULL();
  }

  if (GAB_VAL_IS_STRING(argv[0])) {
    gab_obj_string *src = GAB_VAL_TO_STRING(argv[0]);
    f64 offsetf = GAB_VAL_TO_NUMBER(argv[1]);
    f64 sizef = GAB_VAL_TO_NUMBER(argv[2]);
    u64 offset = offsetf;
    u64 size = sizef;

    if (offset + size > src->size) {
      return GAB_VAL_NULL();
    }

    s_u8_ref result = s_u8_ref_create(src->data + offset, size);

    return GAB_VAL_OBJ(gab_obj_string_create(eng, result));
  }

  // TODO: Handle slicing objects

  return GAB_VAL_NULL();
};

gab_value gab_lib_len(gab_engine *eng, gab_value *argv, u8 argc) {
  if (GAB_VAL_IS_OBJECT(argv[0])) {
    gab_obj_object *obj = GAB_VAL_TO_OBJECT(argv[0]);

    return GAB_VAL_NUMBER(obj->is_dynamic ? obj->dynamic_values.size
                                          : obj->static_size);
  }

  if (GAB_VAL_IS_STRING(argv[0])) {
    gab_obj_string *obj = GAB_VAL_TO_STRING(argv[0]);

    return GAB_VAL_NUMBER(obj->size);
  }

  return GAB_VAL_NULL();
}

gab_value gab_lib_push(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_OBJECT(argv[0])) {
    return GAB_VAL_NULL();
  }

  if (argc < 2) {
    return GAB_VAL_NULL();
  }

  gab_obj_object *obj = GAB_VAL_TO_OBJECT(argv[0]);

  for (u8 i = 1; i < argc; i++) {
    u64 index = obj->is_dynamic ? obj->dynamic_values.size : obj->static_size;
    gab_obj_object_insert(obj, eng, GAB_VAL_NUMBER(index), argv[i]);
  }

  return GAB_VAL_OBJ(obj);
}
