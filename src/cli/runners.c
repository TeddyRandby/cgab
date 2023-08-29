#include "builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include <stdio.h>

void repl(const char *module, uint8_t flags) {
  gab_eg *gab = gab_create(0, NULL, NULL);

  gab_setup_builtins(gab);

  a_gab_value *result = NULL;

  if (module != NULL)
    result = gab_send(gab, NULL, gab_string(gab, "require"),
                      gab_string(gab, module), 0, NULL);
  else
    result = gab_send(gab, NULL, gab_string(gab, "require"),
                      gab_string(gab, "std"), 0, NULL);

  a_gab_value_destroy(result);

  uint64_t index = gab_argpush(gab, gab_string(gab, "it"));

  gab_argput(gab, gab_nil, index);

  for (;;) {
    printf("grepl: ");
    a_int8_t *src = os_read_fd_line(stdin);

    if (src->data[0] == EOF) {
      a_int8_t_destroy(src);
      goto fin;
    }

    if (src->data[1] == '\0') {
      a_int8_t_destroy(src);
      continue;
    }

    gab_value main = gab_block(gab, (struct gab_block_argt){
                                        .name = "__main__",
                                        .source = (char *)src->data,
                                        flags,
                                    });

    a_int8_t_destroy(src);

    if (main == gab_undefined)
      continue;

    a_gab_value *result = gab_run(gab, main, flags);

    printf("=> ");
    for (int32_t i = 0; i < result->len; i++) {
      gab_value arg = result->data[i];

      if (i == result->len - 1)
        printf("%V\n", arg);
      else
        printf("%V, ", arg);

      gab_egkeep(gab, arg);
    }

    gab_argput(gab, result->data[0], index);

    a_gab_value_destroy(result);
  }

fin:
#if GAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}

void run_src(gab_eg *gab, s_int8_t src, const char *module, char delim, uint8_t flags) {
  if (module != NULL) {
    a_gab_value *res = gab_send(gab, NULL, gab_string(gab, "require"),
                                gab_string(gab, module), 0, NULL);
    a_gab_value_destroy(res);
  }

  gab_value main = gab_block(gab, (struct gab_block_argt){
                                      .name = "__main__",
                                      .source = (char *)src.data,
                                      .flags = flags,
                                  });

  if (main == gab_undefined)
    return;

  gab_egkeep(gab, main);

  if (flags & fGAB_STREAM_INPUT) {

    if (delim == 0)
      delim = ' ';

    for (;;) {
      a_int8_t *line = os_read_fd_line(stdin);

      if (line->data[0] == EOF || line->data[0] == '\0')
        break;

      uint64_t offset = 0;
      uint64_t nargs = 0;

      // Skip the \n and the \0
      s_int8_t line_s = s_int8_t_create(line->data, line->len - 2);

      for (;;) {
        s_int8_t arg = s_int8_t_tok(line_s, offset, delim);

        if (arg.len == 0)
          break;

        gab_value arg_val = gab_nstring(gab, arg.len, (const char *)arg.data);

        gab_argput(gab, arg_val, gab_argpush(gab, gab_string(gab, "")));

        offset += arg.len + 1;
        nargs++;
      }

      a_gab_value *result = gab_run(gab, main, flags | fGAB_EXIT_ON_PANIC);

      for (int32_t i = 0; i < result->len; i++) {
        gab_value arg = result->data[i];

        gab_egkeep(gab, arg);
      }

      a_gab_value_destroy(result);

      while (nargs--)
        gab_argpop(gab);
    }

    return;
  }

  a_gab_value *result = gab_run(gab, main, flags | fGAB_EXIT_ON_PANIC);

  for (int32_t i = 0; i < result->len; i++) {
    gab_value arg = result->data[i];
    gab_egkeep(gab, arg);
  }

  a_gab_value_destroy(result);
}

void run_string(const char *string, const char *module, char delim, uint8_t flags) {
  gab_eg *gab = gab_create(0, NULL, NULL);

  gab_setup_builtins(gab);

  // This is a weird case where we actually want to include the null terminator
  s_int8_t src = s_int8_t_create((int8_t *)string, strlen(string) + 1);

  run_src(gab, src, module, delim, flags);

#if cGAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}

void run_file(const char *path, const char *module, char delim, uint8_t flags) {
  gab_eg *gab = gab_create(0, NULL, NULL);

  gab_setup_builtins(gab);

  a_int8_t *src = os_read_file(path);

  run_src(gab, s_int8_t_create(src->data, src->len), module, delim, flags);

  a_int8_t_destroy(src);

#if cGAB_DEBUG_GC
  gab_destroy(gab);
#endif
  return;
}
