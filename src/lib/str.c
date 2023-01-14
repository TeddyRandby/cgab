#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>
#include <string.h>

gab_value gab_lib_slice(gab_engine *gab, gab_vm *vm, gab_value receiver,
                        u8 argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(receiver) ||
      !GAB_VAL_IS_NUMBER(argv[0]) || !GAB_VAL_IS_NUMBER(argv[1])) {
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

  return GAB_VAL_OBJ(gab_obj_string_create(gab, result));
};

gab_value gab_lib_split(gab_engine *gab, gab_vm *vm, gab_value receiver,
                        u8 argc, gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_IS_STRING(receiver) ||
      !GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *src = GAB_VAL_TO_STRING(receiver);
  gab_obj_string *delim_src = GAB_VAL_TO_STRING(argv[0]);

  s_i8 delim = gab_obj_string_ref(delim_src);
  s_i8 window = s_i8_create(src->data, delim.len);

  i8 *start = window.data;
  u64 len = 0;

  v_u64 splits;
  v_u64_create(&splits, 8);

  // printf("delim: %.*s\n", (i32)delim.len, delim.data);
  while (window.data + window.len < src->data + src->len) {
    // printf("window: %.*s\n", (i32)window.len, window.data);
    if (s_i8_match(window, delim)) {
      s_i8 split = s_i8_create(start, len);

      printf("%.*s (%lu)\n", (i32) split.len, split.data, split.len);

      v_u64_push(&splits, GAB_VAL_OBJ(gab_obj_string_create(gab, split)));

      window.data += window.len;
      start = window.data;
      len = 0;
    } else {
      len++;
      window.data++;
    }
  }

  gab_value result = GAB_ARRAY(vm, splits.len, splits.data);

  v_u64_destroy(&splits);

  return result;
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value string_type = gab_get_type(gab, TYPE_STRING);

  s_i8 keys[] = {
      s_i8_cstr("slice"),
      s_i8_cstr("split"),
  };

  gab_value values[] = {
      GAB_BUILTIN(slice),
      GAB_BUILTIN(split),
  };

  for (u8 i = 0; i < LEN_CARRAY(keys); i++) {
    gab_specialize(gab, keys[i], string_type, values[i]);
  }

  return GAB_VAL_NULL();
}
