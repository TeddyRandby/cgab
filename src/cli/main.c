#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/os.h"
#include "print.h"
#include "require.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void bind_std(gab_engine *gab) {
  s_i8 keys[] = {s_i8_cstr("print"), s_i8_cstr("require")};

  gab_value values[] = {
      GAB_BUILTIN(print),
      GAB_BUILTIN(require),
  };

  gab_bind(gab, sizeof(values) / sizeof(gab_value), keys, values);
}

void gab_run_file(const char *path) {
  imports_create();

  gab_engine *gab = gab_create(GAB_FLAG_DUMP_ERROR);
  bind_std(gab);

  a_i8 *src = os_read_file(path);

  gab_value pkg =
      gab_compile(gab, s_i8_cstr("__main__"), s_i8_create(src->data, src->len));

  gab_value result = GAB_VAL_NULL();

  if (GAB_VAL_IS_NULL(pkg)) {
    goto fin;
  }

  result = gab_run_main(gab, pkg);

fin:
  gab_dref(gab, result);
  gab_dref(gab, pkg);
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
