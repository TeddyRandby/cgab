#include "../core/os.h"
#include "../gab/gab.h"
#include "print.h"
#include "require.h"
#include "src/core/core.h"
#include "src/gab/engine.h"
#include "src/gab/object.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

a_i8 *collect_line() {
  printf(">>> ");
  return os_read_line();
}

#define GAB_BUILTIN(name, arity)                                               \
  GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_##name, #name, arity))

void bind_std(gab_engine *gab) {
  s_i8 keys[] = {s_i8_cstr("print"), s_i8_cstr("require")};

  gab_value values[] = {
      GAB_BUILTIN(print, VAR_RET),
      GAB_BUILTIN(require, 1),
  };

  gab_bind(gab, 2, keys, values);
}

void gab_repl() {
  a_i8 *error = a_i8_empty(4096);
  gab_engine *gab = gab_create(error);
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
      gab_result result =
          gab_run(gab, s_i8_cstr("__repl__"), s_i8_create(src->data, src->len),
                  GAB_FLAG_NONE);

      if (!gab_result_ok(result)) {
        fprintf(stderr, "%.*s", (i32)gab->error->len, gab->error->data);
      } else {
        gab_value mod = gab_result_value(result);
        gab_val_dump(mod);
        printf("\n");
      }
    }

    a_i8_destroy(src);
  }

  gab_dref(gab, gab->std);
  gab_destroy(gab);
  a_i8_destroy(error);
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
