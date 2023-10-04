#include "include/builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include <stdio.h>

void repl(const char *module, uint8_t flags) {
  struct gab_eg *gab = gab_create();

  a_gab_value *result = NULL;

  // if (module != NULL)
  //   result = gab_send(gab, NULL, gab_string(gab, "require"),
  //                     gab_string(gab, module), 0, NULL);

  a_gab_value_destroy(result);

  uint64_t index = gab_argpush(gab, gab_string(gab, "it"));

  gab_argput(gab, gab_nil, index);

  for (;;) {
    printf("grepl: ");
    a_char *src = os_read_fd_line(stdin);

    if (src->data[0] == EOF) {
      a_char_destroy(src);
      goto fin;
    }

    if (src->data[1] == '\0') {
      a_char_destroy(src);
      continue;
    }

    gab_value main = gab_compile(gab, (struct gab_compile_argt){
                                        .name = "__main__",
                                        .source = (char *)src->data,
                                        flags,
                                    });

    a_char_destroy(src);

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
  gab_destroy(gab);
  return;
}

void run_src(struct gab_eg *gab, s_char src, const char *module, char delim,
             uint8_t flags) {
  // if (module != NULL) {
  //   a_gab_value *res = gab_send(gab, NULL, gab_string(gab, "require"),
  //                               gab_string(gab, module), 0, NULL);
  //   a_gab_value_destroy(res);
  // }

  gab_value main = gab_compile(gab, (struct gab_compile_argt){
                                      .name = "__main__",
                                      .source = (char *)src.data,
                                      .flags = flags,
                                  });

  if (main == gab_undefined)
    return;

  if (flags & fGAB_STREAM_INPUT) {

    if (delim == 0)
      delim = ' ';

    for (;;) {
      a_char *line = os_read_fd_line(stdin);

      if (line->data[0] == EOF || line->data[0] == '\0')
        break;

      uint64_t offset = 0;
      uint64_t nargs = 0;

      // Skip the \n and the \0
      s_char line_s = s_char_create(line->data, line->len - 2);

      for (;;) {
        s_char arg = s_char_tok(line_s, offset, delim);

        if (arg.len == 0)
          break;

        gab_value arg_val = gab_nstring(gab, arg.len, (const char *)arg.data);

        gab_argput(gab, arg_val, gab_argpush(gab, gab_string(gab, "")));

        offset += arg.len + 1;
        nargs++;
      }

      a_gab_value *result = gab_run(gab, main, flags | fGAB_EXIT_ON_PANIC);

      gab_negkeep(gab, result->len, result->data);

      a_gab_value_destroy(result);

      while (nargs--)
        gab_argpop(gab);
    }

    return;
  }

  a_gab_value *result = gab_run(gab, main, flags | fGAB_EXIT_ON_PANIC);

  gab_negkeep(gab, result->len, result->data);
  a_gab_value_destroy(result);
}

void run_string(const char *string, const char *module, char delim,
                uint8_t flags) {
  struct gab_eg *gab = gab_create();

  // This is a weird case where we actually want to include the null terminator
  s_char src = s_char_create(string, strlen(string) + 1);

  run_src(gab, src, module, delim, flags);

  gab_destroy(gab);
  return;
}

void run_file(const char *path, const char *module, char delim, uint8_t flags) {
  struct gab_eg *gab = gab_create();

  a_char *src = os_read_file(path);

  run_src(gab, s_char_create(src->data, src->len), module, delim, flags);

  a_char_destroy(src);

  gab_destroy(gab);
  return;
}
