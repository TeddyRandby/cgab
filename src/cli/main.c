#include "builtins.h"
#include "include/alloc.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/os.h"
#include <assert.h>
#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gab_value make_panic(gab_engine *gab) {
  gab_value panic_builtin = GAB_BUILTIN(panic);

  return panic_builtin;
}

gab_value make_print(gab_engine *gab) {
  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_BOOLEAN),
      gab_type(gab, GAB_KIND_NUMBER),
      GAB_VAL_NIL(),
      gab_type(gab, GAB_KIND_STRING),
      gab_type(gab, GAB_KIND_CONTAINER),
      gab_type(gab, GAB_KIND_SHAPE),
      gab_type(gab, GAB_KIND_BLOCK),
      gab_type(gab, GAB_KIND_MESSAGE),
      gab_type(gab, GAB_KIND_EFFECT),
  };

  gab_value print_builtin = GAB_BUILTIN(print);

  for (i32 i = 0; i < LEN_CARRAY(receivers); i++) {
    gab_specialize(gab, GAB_STRING("print"), receivers[i], print_builtin);
  }

  return print_builtin;
}

gab_value make_require(gab_engine *gab) {
  gab_value require_builtin = GAB_BUILTIN(require);

  gab_specialize(gab, GAB_STRING("require"), gab_type(gab, GAB_KIND_STRING),
                 require_builtin);

  return require_builtin;
}

void gab_repl() {
  imports_create();

  gab_engine *gab = gab_create(alloc_setup());

  gab_value arg_names[] = {
      GAB_STRING("print"),  GAB_STRING("require"), GAB_STRING("panic"),
      GAB_STRING("String"), GAB_STRING("Number"),  GAB_STRING("Boolean"),
      GAB_STRING("Block"),  GAB_STRING("Message"), GAB_STRING("Effect"),
      GAB_STRING("Record"), GAB_STRING("List"),    GAB_STRING("Map"),
      GAB_STRING("it")};

  gab_value args[] = {
      make_print(gab),
      make_require(gab),
      make_panic(gab),
      gab_type(gab, GAB_KIND_STRING),
      gab_type(gab, GAB_KIND_NUMBER),
      gab_type(gab, GAB_KIND_BOOLEAN),
      gab_type(gab, GAB_KIND_BLOCK),
      gab_type(gab, GAB_KIND_MESSAGE),
      gab_type(gab, GAB_KIND_EFFECT),
      gab_type(gab, GAB_KIND_RECORD),
      gab_type(gab, GAB_KIND_LIST),
      gab_type(gab, GAB_KIND_MAP),
      GAB_VAL_NIL(),
  };

  static_assert(LEN_CARRAY(arg_names) == LEN_CARRAY(args));

  gab_args(gab, LEN_CARRAY(arg_names), arg_names, args);

  // Import the standard library
  GAB_SEND("require", GAB_STRING("std"), 0, NULL);

  for (;;) {

    printf("grepl> ");
    a_i8 *src = os_read_line();

    if (src == NULL || src->data[0] == '\0') {
      a_i8_destroy(src);
      break;
    }

    if (src->data[1] == '\0') {
      a_i8_destroy(src);
      continue;
    }

    gab_module *main =
        gab_compile(gab, GAB_STRING("__main__"),
                    s_i8_create(src->data, src->len), GAB_FLAG_DUMP_ERROR);

    a_i8_destroy(src);

    if (main == NULL)
      continue;

    gab_value result = gab_run(gab, main, GAB_FLAG_DUMP_ERROR);

    if (!GAB_VAL_IS_NIL(result)) {
      printf("%V\n", result);
    }

    gab_dref(gab, NULL, args[LEN_CARRAY(args) - 1]);

    args[LEN_CARRAY(args) - 1] = result;

    gab_args(gab, LEN_CARRAY(arg_names), arg_names, args);
  }

  gab_dref_many(gab, NULL, 3, args);

  void *ud = gab_user(gab);

  gab_destroy(gab);

  alloc_teardown(ud);

  imports_destroy(gab);
}

void gab_run_file(const char *path) {
  imports_create();

  gab_engine *gab = gab_create(alloc_setup());

  gab_value arg_names[] = {
      GAB_STRING("print"),  GAB_STRING("require"), GAB_STRING("panic"),
      GAB_STRING("String"), GAB_STRING("Number"),  GAB_STRING("Boolean"),
      GAB_STRING("Block"),  GAB_STRING("Message"), GAB_STRING("Effect"),
      GAB_STRING("Record"), GAB_STRING("List"),    GAB_STRING("Map"),
  };

  gab_value args[] = {
      make_print(gab),
      make_require(gab),
      make_panic(gab),
      gab_type(gab, GAB_KIND_STRING),
      gab_type(gab, GAB_KIND_NUMBER),
      gab_type(gab, GAB_KIND_BOOLEAN),
      gab_type(gab, GAB_KIND_BLOCK),
      gab_type(gab, GAB_KIND_MESSAGE),
      gab_type(gab, GAB_KIND_EFFECT),
      gab_type(gab, GAB_KIND_RECORD),
      gab_type(gab, GAB_KIND_LIST),
      gab_type(gab, GAB_KIND_MAP),
  };

  static_assert(LEN_CARRAY(arg_names) == LEN_CARRAY(args));

  gab_args(gab, LEN_CARRAY(arg_names), arg_names, args);

  a_i8 *src = os_read_file(path);

  gab_module *main =
      gab_compile(gab, GAB_STRING("__main__"), s_i8_create(src->data, src->len),
                  GAB_FLAG_DUMP_ERROR);

  a_i8_destroy(src);

  if (main == NULL)
    goto fin;

  gab_value result =
      gab_run(gab, main, GAB_FLAG_DUMP_ERROR | GAB_FLAG_EXIT_ON_PANIC);

  gab_dref(gab, NULL, result);

fin : {
  gab_dref_many(gab, NULL, 3, args);

  void *ud = gab_user(gab);
  gab_destroy(gab);
  alloc_teardown(ud);
  imports_destroy(gab);
}
}

i32 main(i32 argc, const char **argv) {
  register_printf_specifier('V', gab_val_printf_handler,
                            gab_val_printf_arginfo);

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
