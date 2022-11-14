#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/os.h"
#include "print.h"
#include "require.h"
#include <stdio.h>

s_i8 global_arg_names[GLOBAL_ARG_LEN];
gab_value global_arg_values[GLOBAL_ARG_LEN];

gab_value make_print(gab_engine *gab) {
  gab_value receivers[] = {
      gab_get_type(gab, TYPE_BOOLEAN),
      gab_get_type(gab, TYPE_NUMBER),
      GAB_VAL_NULL(),
      gab_get_type(gab, TYPE_STRING),
      gab_get_type(gab, TYPE_SYMBOL),
      gab_get_type(gab, TYPE_CONTAINER),
      gab_get_type(gab, TYPE_SHAPE),
      gab_get_type(gab, TYPE_FUNCTION),
  };

  gab_value print_builtin = GAB_BUILTIN(print);

  gab_value specializations[] = {
      print_builtin, print_builtin, print_builtin, print_builtin,
      print_builtin, print_builtin, print_builtin, print_builtin,
  };

  return gab_bundle_function(gab, NULL, s_i8_cstr("print"),
                             LEN_CARRAY(receivers), receivers, specializations);
}

gab_value make_require(gab_engine *gab) {
  gab_value receivers[] = {
      GAB_VAL_NULL(),
  };

  gab_value require_builtin = GAB_BUILTIN(require);

  gab_value specializations[] = {
      require_builtin,
  };

  return gab_bundle_function(gab, NULL, s_i8_cstr("require"),
                             LEN_CARRAY(receivers), receivers, specializations);
}

void gab_run_file(const char *path) {
  imports_create();

  gab_engine *gab = gab_create(GAB_FLAG_DUMP_ERROR);

  global_arg_names[0] = s_i8_cstr("print");
  global_arg_names[1] = s_i8_cstr("require");
  global_arg_names[2] = s_i8_cstr("boolean");
  global_arg_names[3] = s_i8_cstr("number");
  global_arg_names[4] = s_i8_cstr("null");
  global_arg_names[5] = s_i8_cstr("string");
  global_arg_names[6] = s_i8_cstr("function");

  global_arg_values[0] = make_print(gab);
  global_arg_values[1] = make_require(gab);
  global_arg_values[2] = gab_get_type(gab, TYPE_BOOLEAN);
  global_arg_values[3] = gab_get_type(gab, TYPE_NUMBER);
  global_arg_values[4] = gab_get_type(gab, TYPE_NULL);
  global_arg_values[5] = gab_get_type(gab, TYPE_STRING);
  global_arg_values[6] = gab_get_type(gab, TYPE_FUNCTION);

  a_i8 *src = os_read_file(path);

  gab_module *main =
      gab_compile(gab, s_i8_cstr("__main__"), s_i8_create(src->data, src->len),
                  GLOBAL_ARG_LEN, global_arg_names);

  gab_value result = GAB_VAL_NULL();

  if (main == NULL) {
    goto fin;
  }

  result = gab_run(gab, main, GLOBAL_ARG_LEN, global_arg_values);

  gab_module_destroy(gab, main);
fin:
  gab_dref(gab, NULL, global_arg_values[0]);
  gab_dref(gab, NULL, global_arg_values[1]);
  gab_dref(gab, NULL, result);
  gab_destroy(gab);
  a_i8_destroy(src);
  imports_destroy();
}

i32 main(i32 argc, const char **argv) {
  if (argc == 2) {
    gab_run_file(argv[1]);
  } else {
    fprintf(stderr, "Usage: gab [path]\n");
    exit(64);
  }
  return 0;
}
