#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>
#include <string.h>

void gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);

  gab_value result = GAB_VAL_NUMBER(str->len);

  gab_push(vm, 1, &result);
};

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

void gab_lib_slice(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {

  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);

  u64 len = str->len;
  u64 start = 0, end = len;

  switch (argc) {
  case 2:
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;

  case 3:
    if (GAB_VAL_IS_NUMBER(argv[1])) {
      start = CLAMP(GAB_VAL_TO_NUMBER(argv[1]), len);
    } else if (!GAB_VAL_IS_NIL(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    if (GAB_VAL_TO_NUMBER(argv[2])) {
      end = CLAMP(GAB_VAL_TO_NUMBER(argv[2]), len);
    } else if (!GAB_VAL_IS_NIL(argv[2])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }
    break;

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  gab_obj_string *src = GAB_VAL_TO_STRING(argv[0]);

  u64 size = end - start;

  s_i8 result = s_i8_create(src->data + start, size);

  gab_value res = GAB_VAL_OBJ(gab_obj_string_create(gab, result));

  gab_push(vm, 1, &res);
};

void gab_lib_split(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_split");
    return;
  }

  gab_obj_string *src = GAB_VAL_TO_STRING(argv[0]);
  gab_obj_string *delim_src = GAB_VAL_TO_STRING(argv[1]);

  s_i8 delim = gab_obj_string_ref(delim_src);
  s_i8 window = s_i8_create(src->data, delim.len);

  i8 *start = window.data;
  u64 len = 0;

  v_u64 splits;
  v_u64_create(&splits, 8);

  while (window.data + window.len <= src->data + src->len) {
    if (s_i8_match(window, delim)) {
      s_i8 split = s_i8_create(start, len);

      v_u64_push(&splits, GAB_VAL_OBJ(gab_obj_string_create(gab, split)));

      window.data += window.len;
      start = window.data;
      len = 0;
    } else {
      len++;
      window.data++;
    }
  }

  s_i8 split = s_i8_create(start, len);
  v_u64_push(&splits, GAB_VAL_OBJ(gab_obj_string_create(gab, split)));

  gab_obj_shape* shape = gab_obj_shape_create_tuple(gab, vm, splits.len);

  gab_obj_record* record = gab_obj_record_create(gab, vm, shape, 1, splits.data);

  gab_value result = GAB_VAL_OBJ(record);

  gab_push(vm, 1, &result);

  gab_val_dref(vm, result);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value string_type = gab_type(gab, GAB_KIND_STRING);

  gab_value keys[] = {
      GAB_STRING("slice"),
      GAB_STRING("split"),
      GAB_STRING("len"),
  };

  gab_value values[] = {
      GAB_BUILTIN(slice),
      GAB_BUILTIN(split),
      GAB_BUILTIN(len),
  };

  for (u8 i = 0; i < LEN_CARRAY(keys); i++) {
    gab_specialize(gab, vm, keys[i], string_type, values[i]);
    gab_val_dref(vm, values[i]);
  }

  return GAB_VAL_NIL();
}
