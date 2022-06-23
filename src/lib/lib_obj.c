#include "lib.h"

gab_value gab_lib_keys(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_OBJECT(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_object *obj = GAB_VAL_TO_OBJECT(argv[0]);

  u64 size = obj->is_dynamic ? obj->dynamic_values.size : obj->static_size;

  gab_obj_shape *shape = gab_obj_shape_create_arr(eng, size);

  gab_obj_object *list =
      gab_obj_object_create(eng, shape, obj->shape->keys, size, 1);

  gab_gc_push_dec_obj_ref(eng->gc, (gab_obj *)list);

  return GAB_VAL_OBJ(list);
}

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
