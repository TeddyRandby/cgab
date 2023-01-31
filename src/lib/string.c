#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>
#include <string.h>

gab_value gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
  }

  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);

  return GAB_VAL_NUMBER(str->len);
};

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

gab_value gab_lib_slice(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
  }

  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);

  u64 len = str->len;
  u64 start = 0, end = len;

  switch (argc) {

  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;
  }

  case 3: {
    if (GAB_VAL_IS_NUMBER(argv[1])) {
      start = CLAMP(GAB_VAL_TO_NUMBER(argv[1]), len);
    } else if (!GAB_VAL_IS_NIL(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    }
    if (GAB_VAL_TO_NUMBER(argv[2])) {
      end = CLAMP(GAB_VAL_TO_NUMBER(argv[2]), len);
    } else if (!GAB_VAL_IS_NIL(argv[2])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    }
    break;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
  }

  gab_obj_string *src = GAB_VAL_TO_STRING(argv[0]);

  u64 size = end - start;

  s_i8 result = s_i8_create(src->data + start, size);

  return GAB_VAL_OBJ(gab_obj_string_create(gab, result));
};

gab_value gab_lib_split(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_split");
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

  gab_value result = GAB_ARRAY(vm, splits.len, splits.data);

  v_u64_destroy(&splits);

  return result;
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value string_type = gab_get_type(gab, TYPE_STRING);

  s_i8 keys[] = {
      s_i8_cstr("slice"),
      s_i8_cstr("split"),
      s_i8_cstr("len"),
  };

  gab_value values[] = {
      GAB_BUILTIN(slice),
      GAB_BUILTIN(split),
      GAB_BUILTIN(len),
  };

  for (u8 i = 0; i < LEN_CARRAY(keys); i++) {
    gab_specialize(gab, keys[i], string_type, values[i]);
    gab_dref(gab, vm, values[i]);
  }

  return GAB_VAL_NIL();
}
