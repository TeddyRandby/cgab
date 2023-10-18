#include "include/builtins.h"
#include "include/gab.h"
#include "include/os.h"
#include <stdio.h>

#define MAIN_MODULE "__main__"

void repl(const char *module, int flags) {
  struct gab_triple gab = gab_create();

  a_gab_value *result = NULL;

  // if (module != NULL)
  //   result = gab_send(gab, NULL, gab_string(gab, "require"),
  //                     gab_string(gab, module), 0, NULL);

  a_gab_value_destroy(result);

  gab_value prev = gab_nil;

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

    a_gab_value *result = gab_exec(gab, (struct gab_exec_argt){
                                            .name = MAIN_MODULE,
                                            .source = (char *)src->data,
                                            .flags = flags,
                                            .len = 1,
                                            .sargv = (const char *[]){"_"},
                                            .argv = &prev,
                                        });

    if (result == NULL)
      continue;

    printf("=> ");
    for (int32_t i = 0; i < result->len; i++) {
      gab_value arg = result->data[i];

      if (i == result->len - 1)
        printf("%V\n", arg);
      else
        printf("%V, ", arg);

      gab_egkeep(gab.eg, arg);
    }

    prev = result->data[0];

    a_gab_value_destroy(result);
  }

fin:
  gab_destroy(gab);
  return;
}

void run_src(struct gab_triple gab, s_char src, const char *module, char delim,
             uint8_t flags) {
  // if (module != NULL) {
  //   a_gab_value *res = gab_send(gab, NULL, gab_string(gab, "require"),
  //                               gab_string(gab, module), 0, NULL);
  //   a_gab_value_destroy(res);
  // }

  gab_value main = gab_cmpl(gab, (struct gab_cmpl_argt){
                                     .name = MAIN_MODULE,
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

      uint64_t offset = 0, nargs = 0;

      // Skip the \n and the \0
      s_char line_s = s_char_create(line->data, line->len - 2);

      gab_value buf[255];

      for (;;) {
        s_char arg = s_char_tok(line_s, offset, delim);

        if (arg.len == 0)
          break;

        buf[nargs] = gab_nstring(gab.eg, arg.len, (const char *)arg.data);

        offset += arg.len + 1;
        nargs++;
      }

      a_gab_value *result = gab_run(gab, (struct gab_run_argt){
                                             .main = main,
                                             .flags = flags,
                                             .len = nargs,
                                             .argv = buf,
                                         });

      gab_negkeep(gab.eg, result->len, result->data);

      a_gab_value_destroy(result);
    }

    return;
  }

  a_gab_value *result = gab_run(gab, (struct gab_run_argt){
                                         .main = main,
                                         .flags = flags,
                                     });

  gab_negkeep(gab.eg, result->len, result->data);

  a_gab_value_destroy(result);
}

void run_string(const char *string, const char *module, char delim,
                uint8_t flags) {
  struct gab_triple gab = gab_create();

  // This is a weird case where we actually want to include the null terminator
  s_char src = s_char_create(string, strlen(string) + 1);

  run_src(gab, src, module, delim, flags);

  gab_destroy(gab);
  return;
}

void run_file(const char *path, const char *module, char delim, uint8_t flags) {
  struct gab_triple gab = gab_create();

  a_char *src = os_read_file(path);

  run_src(gab, s_char_create(src->data, src->len), module, delim, flags);

  a_char_destroy(src);

  gab_destroy(gab);
  return;
}
