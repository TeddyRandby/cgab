#include "builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include "include/value.h"
#include <stdio.h>

void gab_repl(const char *module, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab, NULL);

  a_gab_value *result = NULL;

  if (module != NULL)
    result =
        gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);
  else
    result =
        gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING("std"), 0, NULL);

  a_gab_value_destroy(result);

  u64 index = gab_arg_push(gab, GAB_STRING("it"));

  gab_arg_set(gab, GAB_VAL_NIL(), index);

  for (;;) {
    printf("grepl: ");
    a_i8 *src = os_read_fd_line(stdin);

    if (src->data[0] == EOF) {
      a_i8_destroy(src);
      goto fin;
    }

    if (src->data[1] == '\0') {
      a_i8_destroy(src);
      continue;
    }

    gab_value main = gab_compile(gab, GAB_STRING("__main__"),
                                 s_i8_create(src->data, src->len), flags);

    a_i8_destroy(src);

    if (GAB_VAL_IS_UNDEFINED(main))
      continue;

    a_gab_value *result = gab_run(gab, main, flags);

    printf("=> ");
    for (i32 i = 0; i < result->len; i++) {
      gab_value arg = result->data[i];

      if (i == result->len - 1)
        printf("%V\n", arg);
      else
        printf("%V, ", arg);

      gab_scratch(gab, arg);
    }

    gab_arg_set(gab, result->data[0], index);

    a_gab_value_destroy(result);
  }

fin:
#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}

void run_src(gab_engine *gab, s_i8 src, u8 flags) {
  gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags);

  if (GAB_VAL_IS_NIL(main))
    return;

  a_gab_value *result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

  for (i32 i = 0; i < result->len; i++) {
    gab_value arg = result->data[i];
    gab_scratch(gab, arg);
  }

  a_gab_value_destroy(result);
}

void gab_run_string(const char *string, const char *module, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab, NULL);

  // This is a weird case where we actually want to include the null terminator
  s_i8 src = s_i8_create((i8 *)string, strlen(string) + 1);

  if (module != NULL) {
    a_gab_value *result =
        gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);

    a_gab_value_destroy(result);
  }

  if (flags & GAB_FLAG_STREAM_INPUT) {
    u64 index = gab_arg_push(gab, GAB_STRING("it"));

    gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags);

    if (GAB_VAL_IS_UNDEFINED(main))
      return;

    gab_scratch(gab, main);

    if (flags & GAB_FLAG_DELIMIT_INPUT) {
      for (;;) {
        a_i8 *it = os_read_fd_line(stdin);

        if (it->data[0] == EOF)
          break;

        gab_value it_val = GAB_VAL_OBJ(
            gab_obj_string_create(gab, s_i8_create(it->data, it->len - 2)));

        gab_arg_set(gab, it_val, index);

        a_gab_value *result =
            gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

        for (i32 i = 0; i < result->len; i++) {
          gab_value arg = result->data[i];

          printf("%V\n", arg);

          gab_scratch(gab, arg);
        }

        a_gab_value_destroy(result);
      }

      goto fin;
    }

    a_i8 *it = os_read_fd(stdin);

    gab_value it_val =
        GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_create(it->data, it->len)));

    gab_arg_set(gab, it_val, index);
  }

  gab_value main = gab_compile(gab, GAB_STRING("__main__"), src,
                               flags); // This is wasteful

  if (GAB_VAL_IS_UNDEFINED(main))
    goto fin;

  a_gab_value *result = gab_run(gab, main, flags | GAB_FLAG_EXIT_ON_PANIC);

  for (i32 i = 0; i < result->len; i++) {
    gab_value arg = result->data[i];

    printf("%V\n", arg);

    gab_scratch(gab, arg);
  }

  a_gab_value_destroy(result);

  gab_scratch(gab, main);

fin:
#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}

void gab_run_file(const char *path, const char *module, u8 flags) {

  gab_engine *gab = gab_create();

  gab_setup_builtins(gab, NULL);

  if (module != NULL) {
    a_gab_value* res = gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);
    a_gab_value_destroy(res);
  }

  a_i8 *src = os_read_file(path);

  run_src(gab, s_i8_create(src->data, src->len), flags);

  a_i8_destroy(src);

#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}
