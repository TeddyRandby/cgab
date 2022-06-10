#include "lib_import.h"

gab_value gab_lib_require(u8 argc, gab_value *argv, gab_engine *eng,
                          char **err) {
  if (argc != 1) {
    *err = "Import expects one argument";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    *err = "Import target must be a string";
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);

  s_u8 *file = gab_io_read_file((char *)path->data);

  gab_result *compile_result = gab_engine_compile(
      eng, file, gab_obj_string_get_ref(path), GAB_FLAG_NONE);

  if (gab_result_has_error(compile_result)) {
    *err = "Import failed";
    gab_result_destroy(compile_result);
    return GAB_VAL_NULL();
  }

  gab_result *run_result = gab_engine_run(eng, compile_result->as.func);

  gab_result_destroy(compile_result);

  if (gab_result_has_error(run_result)) {
    *err = "Import failed";
    gab_result_destroy(run_result);
    return GAB_VAL_NULL();
  }

  gab_value m = run_result->as.result;

  gab_result_destroy(run_result);

  return m;
}
