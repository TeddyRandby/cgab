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
      GAB_BUILTIN(print, VAR_RET),
      GAB_BUILTIN(require, 1),
  };

  gab_bind(gab, 2, keys, values);
}

void gab_run_file(const char *path) {
  a_i8 *error = a_i8_empty(4096);
  a_i8 *src = os_read_file(path);
  gab_engine *gab = gab_create(error);
  bind_std(gab);

  gab_result result = gab_run(gab, s_i8_cstr("__main__"),
                              s_i8_create(src->data, src->len), GAB_FLAG_NONE);

  if (!gab_result_ok(result)) {
    fprintf(stderr, "%.*s", (i32)gab->error->len, gab->error->data);
  }

  gab_dref(gab, gab->std);
  gab_dref(gab, gab_result_value(result));

  gab_destroy(gab);
  a_i8_destroy(src);
  a_i8_destroy(error);
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
