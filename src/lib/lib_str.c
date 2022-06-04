#include "lib_str.h"
#include <string.h>

gab_value gab_lib_str_split(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err) {
  if (argc != 2) {
    *err = "Expected two arguments";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    *err = "Expected string argument to be a string";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[1])) {
    *err = "Expected deliminator argument to be a string";
    return GAB_VAL_NULL();
  }

  gab_obj_string *source = GAB_VAL_TO_STRING(argv[0]);
  gab_obj_string *delim = GAB_VAL_TO_STRING(argv[1]);

  char *data = CREATE_ARRAY(char, source->size + 1);

  strcpy(data, (const char *)source->data);

  data[source->size] = '\0';

  char *tok = strtok(data, (char *)delim->data);

  d_u64 list_data;
  d_u64_create(&list_data, OBJECT_INITIAL_CAP);

  while (tok != NULL) {
    gab_obj_string *tok_to_insert =
        gab_obj_string_create(eng, s_u8_ref_create_cstr(tok));

    gab_gc_push_inc_obj_ref(eng->gc, (gab_obj *)tok_to_insert);

    d_u64_insert(&list_data, GAB_VAL_NUMBER(list_data.size),
                 GAB_VAL_OBJ(tok_to_insert), GAB_VAL_NUMBER(list_data.size));

    tok = strtok(NULL, (char *)delim->data);
  }

  gab_obj_object *list = gab_obj_object_create(eng, NULL, NULL, 0, 0);

  DESTROY_ARRAY(data);

  return GAB_VAL_OBJ(list);
}

gab_value gab_lib_str_tonum(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err) {
  if (argc != 1) {
    *err = "Expected two arguments";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    *err = "Expected string argument to be a string";
    return GAB_VAL_NULL();
  }

  return GAB_VAL_NUMBER(
      strtod((const char *)GAB_VAL_TO_STRING(argv[0])->data, NULL));
};

gab_value gab_lib_str_sub(u8 argc, gab_value *argv, gab_engine *eng,
                          char **err) {
  if (argc != 3) {
    *err = "Expected three arguments";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    *err = "Expected string argument to be a string";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[1])) {
    *err = "Expected index argument to be a number";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[2])) {
    *err = "Expected length argument to be a number";
    return GAB_VAL_NULL();
  }

  gab_obj_string *src = GAB_VAL_TO_STRING(argv[0]);
  f64 offsetf = GAB_VAL_TO_NUMBER(argv[1]);
  f64 sizef = GAB_VAL_TO_NUMBER(argv[2]);
  u64 offset = offsetf;
  u64 size = sizef;

  if (offset + size > src->size) {
    *err = "Substring requested is bigger than the original string";
    return GAB_VAL_NULL();
  }

  s_u8_ref result = s_u8_ref_create(src->data + offset, size);

  return GAB_VAL_OBJ(gab_obj_string_create(eng, result));
};
