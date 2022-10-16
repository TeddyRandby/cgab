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
  gab_obj_shape *mod_shape = gab_obj_shape_create(gab, NULL, 0, 1);

  gab_value mod =
      GAB_VAL_OBJ(gab_obj_record_create(gab, mod_shape, NULL, 0, 1));

  s_i8 keys[] = {s_i8_cstr("print"), s_i8_cstr("require"),
                 s_i8_cstr("__mod__")};

  gab_value values[] = {
      GAB_BUILTIN(print, VAR_RET),
      GAB_BUILTIN(require, 1),
      mod,
  };

  gab_bind(gab, sizeof(values) / sizeof(gab_value), keys, values);
}

void gab_run_file(const char *path) {
  a_i8 *src = os_read_file(path);
  gab_engine *gab = gab_create();
  bind_std(gab);

  gab_value pkg = gab_package_source(gab, s_i8_cstr("__main__"),
                                     s_i8_create(src->data, src->len), GAB_FLAG_DUMP_BYTECODE);

  if (!gab_ok(gab)) {
    fprintf(stderr, "%s", gab_err(gab));
    goto fin;
  }

  gab_value res = gab_run(gab, pkg);
  gab_dref(gab, res);

  if (!gab_ok(gab)) {
    fprintf(stderr, "%s", gab_err(gab));
    goto fin;
  }

fin:
  gab_dref(gab, gab->std);
  gab_destroy(gab);
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
