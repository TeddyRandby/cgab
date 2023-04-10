#include "builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include <stdio.h>

void gab_repl(u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab);

  gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING("std"), 0, NULL);

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

    gab_value main = gab_compile(gab, GAB_STRING("__main__"),
                                 s_i8_create(src->data, src->len), flags);

    a_i8_destroy(src);

    if (GAB_VAL_IS_NIL(main))
      continue;

    gab_value result = gab_run(gab, main, flags);

    if (!GAB_VAL_IS_NIL(result)) {
      printf("%V\n", result);
    }

    gab_scratch(gab, result);
  }

  gab_destroy(gab);
}

void gab_run_string(const char *string, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab);

  s_i8 src = s_i8_create((i8 *)string, strlen(string));

  gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags);

  if (GAB_VAL_IS_NIL(main))
    goto fin;

  gab_value result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

  gab_scratch(gab, main);
  gab_scratch(gab, result);
fin:
  gab_destroy(gab);
}

void gab_run_file(const char *path, u8 flags) {

  gab_engine *gab = gab_create();

  gab_setup_builtins(gab);

  a_i8 *src = os_read_file(path);

  gab_value main = gab_compile(gab, GAB_STRING("__main__"),
                               s_i8_create(src->data, src->len), flags);

  a_i8_destroy(src);

  if (GAB_VAL_IS_NIL(main))
    goto fin;

  gab_value result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

  gab_scratch(gab, main);
  gab_scratch(gab, result);
fin:
  gab_destroy(gab);
}
