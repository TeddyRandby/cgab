#include "include/core.h"
#include "include/gab.h"
#include "include/os.h"
#include "print.h"
#include "require.h"
#include <stdio.h>

gab_value make_print(gab_engine *gab) {
  gab_value receivers[] = {
      gab_get_type(gab, TYPE_BOOLEAN),
      gab_get_type(gab, TYPE_NUMBER),
      GAB_VAL_NULL(),
      gab_get_type(gab, TYPE_STRING),
      gab_get_type(gab, TYPE_SYMBOL),
      gab_get_type(gab, TYPE_CONTAINER),
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

  gab_specialize(gab, s_i8_cstr("require"), GAB_VAL_NULL(),
                        require_builtin);

  return require_builtin;
}

void gab_run_file(const char *path) {
  imports_create();

  gab_engine *gab = gab_create(GAB_FLAG_DUMP_ERROR);

  s_i8 arg_names[] = {
      s_i8_cstr("print"),
      s_i8_cstr("require"),
  };

  gab_value args[] = {
      make_print(gab),
      make_require(gab),
  };

  a_i8 *src = os_read_file(path);

  gab_module *main =
      gab_compile(gab, s_i8_cstr("__main__"), s_i8_create(src->data, src->len),
                  LEN_CARRAY(args), arg_names);

  gab_value result = GAB_VAL_NULL();

  if (main == NULL) {
    goto fin;
  }

  result = gab_run(gab, main, LEN_CARRAY(args), args);

fin:
  // Cleanup the result
  gab_dref(gab, NULL, result);
  for (u8 i = 0; i < LEN_CARRAY(args); i++) {
    gab_dref(gab, NULL, args[i]);
  }

  // Cleanup the engine, module, and imports
  gab_module_cleanup(gab, main);
  gab_cleanup(gab);
  imports_cleanup(gab);

  // Destroy everything
  gab_module_destroy(main);
  gab_destroy(gab);
  imports_destroy();
  a_i8_destroy(src);
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
