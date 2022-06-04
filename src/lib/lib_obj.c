#include "lib_obj.h"

gab_value gab_lib_obj_keys(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err) {
  if (argc != 1) {
    *err = "Expected one argument";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_OBJECT(argv[0])) {
    *err = "Expected object argument to be an object";
    return GAB_VAL_NULL();
  }

  gab_obj_object *obj = GAB_VAL_TO_OBJECT(argv[0]);

  gab_obj_object *list = gab_obj_object_create(eng, NULL, NULL, 0, 0);

  return GAB_VAL_OBJ(list);
}
