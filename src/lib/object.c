#include "include/core.h"
#include "include/gab.h"
#include <assert.h>

gab_value gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
  }

  gab_obj_record *obj = GAB_VAL_TO_RECORD(argv[0]);

  return GAB_VAL_NUMBER(obj->dyn_data.len);
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

gab_value gab_lib_slice(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
  }

  gab_obj_record *r = GAB_VAL_TO_RECORD(argv[0]);

  u64 len = r->dyn_data.len;
  u64 start = 0, end = len;

  switch (argc) {
  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      return GAB_VAL_NIL();
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3:
    if (!GAB_VAL_IS_NUMBER(argv[1]) || !GAB_VAL_TO_NUMBER(argv[2])) {
      return GAB_VAL_NIL();
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    u64 b = GAB_VAL_TO_NUMBER(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  default:
    return GAB_VAL_NIL();
  }

  u64 result_len = end - start;

  gab_value *result_values = r->dyn_data.cap ? r->dyn_data.data : r->data;

  gab_obj_shape *shape =
      gab_obj_shape_create(gab, NULL, result_len, 1, r->shape->keys + start);

  gab_obj_record *record =
      gab_obj_record_create(shape, result_len, 1, result_values + start);

  return GAB_VAL_OBJ(record);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  s_i8 names[] = {
      s_i8_cstr("len"),
      s_i8_cstr("slice"),
  };

  gab_value receivers[] = {
      gab_get_type(gab, TYPE_RECORD),
      gab_get_type(gab, TYPE_RECORD),
  };

  gab_value specs[] = {
      GAB_BUILTIN(len),
      GAB_BUILTIN(slice),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, names[i], receivers[i], specs[i]);
    gab_dref(gab, vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
