#include "../core/os.h"
#include "../gab/gab.h"
#include "print.h"
#include "require.h"
#include "src/gab/engine.h"
#include "src/gab/object.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

a_i8 *collect_line() {
  printf(">>>");
  return os_read_line();
}

void bind_std(gab_engine *gab) {
  gab_lib_kvp builtins[] = {
      GAB_KVP_BUILTIN(print, VAR_RET),
      GAB_KVP_BUILTIN(require, 1),
  };

  gab_bind_library(gab, GAB_KVP_BUNDLESIZE(builtins), builtins);
}

void gab_repl() {
  gab_engine *gab = gab_create();
  bind_std(gab);

  printf("Welcome to Gab v%d.%d\nPress CTRL+D to exit\n", GAB_VERSION_MAJOR,
         GAB_VERSION_MINOR);

  while (true) {

    a_i8 *src = collect_line();

    if (src == NULL) {
      a_i8_destroy(src);
      break;
    }

    if (src->len > 1 && src->data[0] != '\n') {
      gab_result *result = gab_run_source(
          gab, "__repl__", s_i8_create(src->data, src->len), GAB_FLAG_NONE);

      if (gab_result_has_error(result)) {
        gab_result_dump_error(result);
      } else {
        gab_val_dump(result->as.result);
        printf("\n");

        gab_engine_val_dref(gab, result->as.result);
      }

      gab_result_destroy(result);
    }

    a_i8_destroy(src);
  }

  gab_engine_val_dref(gab, gab->std);
  gab_destroy(gab);
}

void gab_run_file(const char *path) {
  a_i8 *src = os_read_file(path);
  gab_engine *gab = gab_create();
  bind_std(gab);

  gab_result *result = gab_run_source(
      gab, "__main__", s_i8_create(src->data, src->len), GAB_FLAG_NONE);

  if (gab_result_has_error(result)) {
    gab_result_dump_error(result);
  }

  a_i8_destroy(src);

  gab_engine_val_dref(gab, gab->std);
  gab_engine_val_dref(gab, result->as.result);

  gab_result_destroy(result);

  gab_destroy(gab);
}

i32 main(i32 argc, const char **argv) {

  if (argc == 1) {
    gab_repl();
  } else if (argc == 2) {
    gab_run_file(argv[1]);
  } else {
    fprintf(stderr, "Usage: gab [path]\n");
    exit(64);
  }

  return 0;
}
