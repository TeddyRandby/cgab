#include "lib.h"

gab_value gab_lib_require(gab_engine *eng, gab_value *argv, u8 argc) {

  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);

  s_u8 *file = gab_io_read_file((char *)path->data);

  gab_result *compile_result = gab_engine_compile(
      eng, file, gab_obj_string_get_ref(path), GAB_FLAG_NONE);

  if (gab_result_has_error(compile_result)) {
    gab_result_destroy(compile_result);
    return GAB_VAL_NULL();
  }

  gab_vm vm;

  gab_vm *old_vm = eng->vm;
  gab_result *run_result = gab_engine_run(eng, &vm, compile_result->as.func);
  eng->vm = old_vm;

  gab_result_destroy(compile_result);

  if (gab_result_has_error(run_result)) {
    gab_result_destroy(run_result);
    return GAB_VAL_NULL();
  }

  gab_value m = run_result->as.result;

  gab_result_destroy(run_result);

  return m;
}
