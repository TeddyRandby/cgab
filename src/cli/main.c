#include "include/gab.h"
#include "include/os.h"
#include "print.h"
#include "require.h"
#include <stdio.h>

gab_value globals = GAB_VAL_NULL();

void make_globals(gab_engine *gab) {
  s_i8 keys[] = {s_i8_cstr("print"), s_i8_cstr("require")};

  gab_value values[] = {
      GAB_BUILTIN(print),
      GAB_BUILTIN(require),
  };

  globals = GAB_RECORD(NULL, sizeof(values) / sizeof(gab_value), keys, values);
}

void gab_run_file(const char *path) {
  imports_create();

  gab_engine *gab = gab_create(GAB_FLAG_DUMP_ERROR);

  a_i8 *src = os_read_file(path);

  make_globals(gab);

  gab_module* main = gab_compile_main(gab, s_i8_create(src->data, src->len));

  gab_value result = GAB_VAL_NULL();

  if (main == NULL) {
    goto fin;
  }

  result = gab_run_main(gab, main, globals);

  gab_module_destroy(gab, main);
fin:
  gab_dref(gab, NULL, globals);
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
