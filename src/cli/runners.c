#include "builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include <stdio.h>

void gab_repl(const char *module, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab, NULL);

  if (module != NULL)
    gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);

  gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING("std"), 0, NULL);

  gab_arg_push(gab, GAB_STRING("it"), GAB_VAL_NIL());

  for (;;) {

    printf("grepl> ");
    a_i8 *src = os_read_fd_line(stdin);

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

    gab_arg_pop(gab);

    gab_arg_push(gab, GAB_STRING("it"), result);
  }

  gab_destroy(gab);
}

void run_src(gab_engine *gab, s_i8 src, u8 flags) {
  gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags);

  if (GAB_VAL_IS_NIL(main))
    goto fin;

  gab_value result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

  gab_scratch(gab, main);
  gab_scratch(gab, result);
fin:
  gab_destroy(gab);
}

void gab_run_string(const char *string, const char *module, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab, NULL);

  s_i8 src = s_i8_create((i8 *)string, strlen(string));

  if (module != NULL)
    gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);

  if (flags & GAB_FLAG_STREAM_INPUT) {
    if (flags & GAB_FLAG_DELIMIT_INPUT) {
      for (;;) {
        a_i8 *it = os_read_fd_line(stdin);

        if (it->len <= 1 || it->data[0] == '\n')
          break;

        gab_value it_val = GAB_VAL_OBJ(
            gab_obj_string_create(gab, s_i8_create(it->data, it->len)));

        gab_arg_push(gab, GAB_STRING("it"), it_val);

        gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags);

        gab_value result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

        gab_scratch(gab, main);
        gab_scratch(gab, result);

        gab_arg_pop(gab);
      }

      goto fin;
    }


    a_i8 *it = os_read_fd(stdin);

    gab_value it_val =
        GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_create(it->data, it->len)));

    gab_arg_push(gab, GAB_STRING("it"), it_val);
  }

  gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags); // This is wasteful

  if (GAB_VAL_IS_NIL(main))
    goto fin;

  gab_value result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

  gab_scratch(gab, main);
  gab_scratch(gab, result);

fin:
  gab_destroy(gab);
}

void gab_run_file(const char *path, const char *module, u8 flags) {

  gab_engine *gab = gab_create();

  gab_setup_builtins(gab, NULL);

  if (module != NULL)
    gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);

  a_i8 *src = os_read_file(path);

  run_src(gab, s_i8_create(src->data, src->len), flags);

  a_i8_destroy(src);
}
