#include "builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include "include/value.h"
#include <stdio.h>

void repl(const char *module, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab);

  a_gab_value *result = NULL;

  if (module != NULL)
    result =
        gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);
  else
    result =
        gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING("std"), 0, NULL);

  a_gab_value_destroy(result);

  u64 index = gab_args_push(gab, GAB_STRING("it"));

  gab_args_put(gab, GAB_VAL_NIL(), index);

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

    gab_args_put(gab, result->data[0], index);

    a_gab_value_destroy(result);
  }

fin:
#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}

void run_src(gab_engine *gab, s_i8 src, const char *module, char delim,
             u8 flags) {
  if (module != NULL) {
    a_gab_value *res =
        gab_send(gab, NULL, GAB_STRING("require"), GAB_STRING(module), 0, NULL);
    a_gab_value_destroy(res);
  }

  gab_value main = gab_compile(gab, GAB_STRING("__main__"), src, flags);

  if (GAB_VAL_IS_UNDEFINED(main))
    return;

  gab_scratch(gab, main);

  if (flags & fGAB_STREAM_INPUT) {

    if (delim == 0)
      delim = ' ';

    for (;;) {
      a_i8 *line = os_read_fd_line(stdin);

      if (line->data[0] == EOF || line->data[0] == '\0')
        break;

      u64 offset = 0;
      u64 nargs = 0;

      // Skip the \n and the \0
      s_i8 line_s = s_i8_create(line->data, line->len - 2);
      
      for (;;) {
        s_i8 arg = s_i8_tok(line_s, offset, delim);

        if (arg.len == 0)
          break;

        gab_value arg_val = GAB_VAL_OBJ(gab_obj_string_create(gab, arg));

        gab_args_put(gab, arg_val, gab_args_push(gab, GAB_STRING("")));

        offset += arg.len + 1;
        nargs++;
      }

      a_gab_value *result = gab_run(gab, main, flags | fGAB_EXIT_ON_PANIC);

      for (i32 i = 0; i < result->len; i++) {
        gab_value arg = result->data[i];

        gab_scratch(gab, arg);
      }

      a_gab_value_destroy(result);

      while (nargs--)
        gab_args_pop(gab);
    }

    return;
  }

  a_gab_value *result = gab_run(gab, main, flags | fGAB_EXIT_ON_PANIC);

  for (i32 i = 0; i < result->len; i++) {
    gab_value arg = result->data[i];
    gab_scratch(gab, arg);
  }

  a_gab_value_destroy(result);
}

void run_string(const char *string, const char *module, char delim, u8 flags) {
  gab_engine *gab = gab_create();

  gab_setup_builtins(gab);

  // This is a weird case where we actually want to include the null terminator
  s_i8 src = s_i8_create((i8 *)string, strlen(string) + 1);

  run_src(gab, src, module, delim, flags);

#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}

void run_file(const char *path, const char *module, char delim, u8 flags) {

  gab_engine *gab = gab_create();

  gab_setup_builtins(gab);

  a_i8 *src = os_read_file(path);

  run_src(gab, s_i8_create(src->data, src->len), module, delim, flags);

  a_i8_destroy(src);

#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}
