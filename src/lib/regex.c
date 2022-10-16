#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include <regex.h>
#include <stdio.h>

gab_value gab_lib_find(gab_engine *eng, gab_value *argv, u8 argc) {
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

  i32 result = regexec(&re, (char *)str->data, 255, &matches[0], 0);

  regfree(&re);

  if (result != 0) {
    return GAB_VAL_NULL();
  }

  gab_obj_record *list = gab_obj_record_create(
      eng, gab_obj_shape_create(eng, NULL, 0, 0), NULL, 0, 0);

  u8 i = 0;
  while (matches[i].rm_so >= 0) {
    s_i8 match = s_i8_create(str->data + matches[i].rm_so,
                             matches[i].rm_eo - matches[i].rm_so);

    gab_value key = GAB_VAL_NUMBER(i);
    gab_value value = GAB_VAL_OBJ(gab_obj_string_create(eng, match));

    gab_obj_record_insert(list, eng, key, value);
    i++;
  }

  return GAB_VAL_OBJ(list);
}

gab_value gab_mod(gab_engine *gab) {
  s_i8 keys[] = {
      s_i8_cstr("find"),
  };

  gab_value values[] = {
      GAB_BUILTIN(find, 2),
  };

  return gab_bundle_record(gab, sizeof(values) / sizeof(gab_value), keys, values);
}
