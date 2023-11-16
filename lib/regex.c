#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/types.h"
#include <regex.h>
#include <stdio.h>

void gab_box_reg_cb(gab_obj_box *self) {
  regfree(self->data);
  DESTROY(self->data);
}

gab_value gab_lib_comp(gab_eg *gab, gab_vm* vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }
  regex_t *re = NEW(regex_t);
  gab_obj_string *pattern = GAB_VAL_TO_STRING(argv[0]);

  if (regcomp(re, (char *)pattern->data, REG_EXTENDED) != 0) {
    return GAB_VAL_NULL();
  }

  return GAB_CONTAINER(gab_container_reg_cb, re);
}

gab_value gab_lib_exec(gab_eg *gab, gab_vm* vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(argv[0]) ||
      !GAB_VAL_IS_BOX(argv[1])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);
  regex_t *re = GAB_VAL_TO_BOX(argv[1])->data;

  regmatch_t matches[255] = {0};

  int32_t result = regexec(re, (char *)str->data, 255, &matches[0], 0);

  if (result != 0) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *list = gab_obj_record_create(
      gab_obj_shape_create(gab, NULL, 0, 0, NULL), 0, 0, NULL);

  uint8_t i = 0;
  while (matches[i].rm_so >= 0) {
    s_char match = s_char_create(str->data + matches[i].rm_so,
                             matches[i].rm_eo - matches[i].rm_so);

    gab_value key = gab_number(i);
    gab_value value = GAB_VAL_OBJ(gab_obj_string_create(gab, match));

    gab_obj_record_insert(gab, vm, list, key, value);
    i++;
  }

  return GAB_VAL_OBJ(list);
}

gab_value gab_lib_find(gab_eg *gab, gab_vm* vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(argv[0]) || !GAB_VAL_IS_STRING(argv[1])) {
    return GAB_VAL_NULL();
  }

  regex_t re;
  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);
  gab_obj_string *pattern = GAB_VAL_TO_STRING(argv[1]);

  if (regcomp(&re, (char *)pattern->data, REG_EXTENDED) != 0) {
    return GAB_VAL_NULL();
  }

  regmatch_t matches[255] = {0};

  int32_t result = regexec(&re, (char *)str->data, 255, &matches[0], 0);

  regfree(&re);

  if (result != 0) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *list = gab_obj_record_create(
      gab_obj_shape_create(gab, NULL, 0, 0, NULL), 0, 0, NULL);

  uint8_t i = 0;
  while (matches[i].rm_so >= 0) {
    s_char match = s_char_create(str->data + matches[i].rm_so,
                             matches[i].rm_eo - matches[i].rm_so);

    gab_value key = gab_number(i);
    gab_value value = GAB_VAL_OBJ(gab_obj_string_create(gab, match));

    gab_obj_record_insert(gab, vm, list, key, value);
    i++;
  }

  return GAB_VAL_OBJ(list);
}

gab_value gab_mod(gab_eg *gab, gab_vm* vm) {
  s_char keys[] = {
      s_char_cstr("find"),
      s_char_cstr("comp"),
      s_char_cstr("exec"),
  };

  gab_value values[] = {
      GAB_NATIVE(find),
      GAB_NATIVE(comp),
      GAB_NATIVE(exec),
  };

  return gab_bundle_record(gab, vm, LEN_CARRAY(values), keys, values);
}
