#include "include/object.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/value.h"

gab_value gab_lib_keys(gab_engine *gab, i32 vm, u8 argc, gab_value argv[argc]) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

  u64 size = obj->is_dynamic ? obj->dynamic_values.len : obj->static_len;

  gab_obj_shape *shape = gab_obj_shape_create_array(gab, size);

  gab_value list =
      GAB_VAL_OBJ(gab_obj_record_create(shape, size, 1, obj->shape->keys));

  return list;
}

gab_value gab_lib_len(gab_engine *gab, i32 vm, u8 argc, gab_value argv[argc]) {
  if (GAB_VAL_IS_RECORD(argv[0])) {
    gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

    return GAB_VAL_NUMBER(obj->is_dynamic ? obj->dynamic_values.len
                                          : obj->static_len);
  }

  if (GAB_VAL_IS_STRING(argv[0])) {
    gab_obj_string *obj = GAB_VAL_TO_STRING(argv[0]);

    return GAB_VAL_NUMBER(obj->len);
  }

  return GAB_VAL_NULL();
}

gab_value gab_lib_pop(gab_engine *gab, i32 vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_IS_RECORD(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

  u64 len = obj->is_dynamic ? obj->dynamic_values.len : obj->static_len;

  gab_obj_shape *s = gab_obj_shape_create(gab, NULL, len, 1, obj->shape->keys);
  gab_obj_record *r = gab_obj_record_create(
      s, len, 1,
      obj->is_dynamic ? obj->dynamic_values.data : obj->static_values);

  return GAB_VAL_OBJ(r);
}

gab_value gab_lib_push(gab_engine *gab, i32 vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_RECORD(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

  for (u8 i = 1; i < argc; i++) {
    u64 index = obj->is_dynamic ? obj->dynamic_values.len : obj->static_len;
    gab_obj_record_insert(gab, obj, GAB_VAL_NUMBER(index), argv[i]);
  }

  return GAB_VAL_OBJ(obj);
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

gab_value gab_lib_slice(gab_engine *gab, i32 vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc < 1 && !GAB_VAL_IS_RECORD(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *r = GAB_VAL_TO_RECORD(argv[0]);

  u64 len = r->is_dynamic ? r->dynamic_values.len : r->static_len;
  u64 start = 0, end = len;

  switch (argc) {
  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      return GAB_VAL_NULL();
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3:
    if (!GAB_VAL_IS_NUMBER(argv[1]) || !GAB_VAL_TO_NUMBER(argv[2])) {
      return GAB_VAL_NULL();
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    u64 b = GAB_VAL_TO_NUMBER(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  default:
    return GAB_VAL_NULL();
  }

  u64 result_len = end - start;

  gab_value *result_values =
      r->is_dynamic ? r->dynamic_values.data : r->static_values;

  gab_obj_shape *shape =
      gab_obj_shape_create(gab, NULL, result_len, 1, r->shape->keys + start);

  gab_obj_record *record =
      gab_obj_record_create(shape, result_len, 1, result_values + start);

  return GAB_VAL_OBJ(record);
}

gab_value gab_mod(gab_engine *gab) {
  s_i8 keys[] = {s_i8_cstr("keys"), s_i8_cstr("len"), s_i8_cstr("pop"),
                 s_i8_cstr("push"), s_i8_cstr("slice")};

  gab_value values[] = {
      GAB_BUILTIN(keys),
      GAB_BUILTIN(len),
      GAB_BUILTIN(pop),
      GAB_BUILTIN(push),
      GAB_BUILTIN(slice),
  };

  return gab_bundle_record(gab, LEN_CARRAY(values), keys, values);
}
