#include "builtins.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/module.h"
#include "include/object.h"
#include "include/os.h"
#include <assert.h>
#include <stdio.h>

gab_value make_print(gab_engine *gab) {
  gab_value receivers[] = {
      gab_get_type(gab, TYPE_BOOLEAN),
      gab_get_type(gab, TYPE_NUMBER),
      GAB_VAL_NIL(),
      gab_get_type(gab, TYPE_STRING),
      gab_get_type(gab, TYPE_SYMBOL), gab_get_type(gab, TYPE_CONTAINER),
      gab_get_type(gab, TYPE_SHAPE),
      gab_get_type(gab, TYPE_CLOSURE),
      gab_get_type(gab, TYPE_MESSAGE),
      gab_get_type(gab, TYPE_EFFECT),
  };

  gab_value print_builtin = GAB_BUILTIN(print);

  for (i32 i = 0; i < LEN_CARRAY(receivers); i++) {
    gab_specialize(gab, s_i8_cstr("print"), receivers[i], print_builtin);
  }

  return print_builtin;
}

gab_value make_require(gab_engine *gab) {
  gab_value require_builtin = GAB_BUILTIN(require);

  gab_specialize(gab, s_i8_cstr("require"), gab_get_type(gab, TYPE_STRING),
                 require_builtin);

  return require_builtin;
}

void gab_run_file(const char *path) {
  imports_create();

  gab_engine *gab = gab_create();

  s_i8 arg_names[] = {
      s_i8_cstr("print"),
      s_i8_cstr("require"),
      s_i8_cstr("panic"),
      // s_i8_cstr("String"),
      // s_i8_cstr("Number"),
      // s_i8_cstr("Boolean"),
  };

  gab_value args[] = {
      make_print(gab),
      make_require(gab),
      GAB_BUILTIN(panic),
      // gab_get_type(gab, TYPE_STRING),
      // gab_get_type(gab, TYPE_NUMBER),
      // gab_get_type(gab, TYPE_BOOLEAN),
  };

  static_assert(LEN_CARRAY(arg_names) == LEN_CARRAY(args));

  gab_args(gab, LEN_CARRAY(arg_names), arg_names, args);

  a_i8 *src = os_read_file(path);

  gab_module *main =
      gab_compile(gab, s_i8_cstr("__main__"), s_i8_create(src->data, src->len),
                  GAB_FLAG_DUMP_ERROR);

  a_i8_destroy(src);

  gab_value result = GAB_VAL_NIL();

  if (main == NULL) {
    goto fin;
  }

  result = gab_run(gab, main, GAB_FLAG_DUMP_ERROR);

fin:
  // Cleanup the result
  gab_dref(gab, NULL, result);
  gab_module_collect(gab, main);
  imports_collect(gab);
  gab_destroy(gab);

  // Destroy everything
  gab_module_destroy(gab, main);
  imports_destroy(gab);
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
