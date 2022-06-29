#include "../gab/gab.h"
#include "../lib/lib.h"

s_u8 *collect_line() {
  printf(">");
  return gab_io_read_line();
}

#define BUILTIN(name, arity)                                                   \
  {                                                                            \
    .key = #name,                                                              \
    .value =                                                                   \
        GAB_VAL_OBJ(gab_obj_builtin_create(gab, gab_lib_##name, #name, arity)) \
  }

#define BUNDLE(name)                                                           \
  {                                                                            \
    .key = #name,                                                              \
    .value = GAB_VAL_OBJ(gab_bundle(                                           \
        gab, sizeof(name##_kvps) / sizeof(gab_lib_kvp), name##_kvps))          \
  }

void bind_std(gab_engine *gab) {

  gab_lib_kvp socket_kvps[] = {BUILTIN(sock, 0),    BUILTIN(bind, 2),
                               BUILTIN(listen, 2),  BUILTIN(accept, 1),
                               BUILTIN(recv, 1),    BUILTIN(send, 2),
                               BUILTIN(connect, 3), BUILTIN(close, 1)};

  gab_lib_kvp re_kvps[] = {BUILTIN(exec, 2)};

  gab_lib_kvp str_kvps[] = {BUILTIN(num, 1)};

  gab_lib_kvp kvps[] = {BUILTIN(info, VAR_RET),
                        BUILTIN(warn, VAR_RET),
                        BUILTIN(error, VAR_RET),
                        BUILTIN(push, VAR_RET),
                        BUILTIN(require, 1),
                        BUILTIN(keys, 1),
                        BUILTIN(len, 1),
                        BUILTIN(slice, 3),
                        BUNDLE(socket),
                        BUNDLE(str),
                        BUNDLE(re)};

  gab_bind_library(gab, sizeof(kvps) / sizeof(gab_lib_kvp), kvps);
}

void gab_repl() {

  gab_engine *gab = gab_create();
  bind_std(gab);

  s_u8 *prev_src = NULL;

  printf("gab v 0.1\n");
  while (true) {

    s_u8 *src = collect_line();

    if (src == NULL || s_u8_match_cstr(src, "exit\n")) {
      break;
    }

    if (src->size > 1 && src->data[0] != '\n') {
      gab_result *result =
          gab_run_source(gab, (char *)src->data, "Main", GAB_FLAG_NONE);

      if (gab_result_has_error(result)) {
        gab_result_dump_error(result);
      } else if (!GAB_VAL_IS_NULL(result->as.result)) {
        gab_val_dump(result->as.result);
        printf("\n");
      }

      gab_result_destroy(result);
    }

    s_u8_destroy(prev_src);
    prev_src = src;
  }

  gab_destroy(gab);
}

void gab_run_file(const char *path) {
  s_u8 *src = gab_io_read_file(path);
  gab_engine *gab = gab_create();
  bind_std(gab);

  gab_result *result =
      gab_run_source(gab, (char *)src->data, "__main__", GAB_FLAG_NONE);

  if (gab_result_has_error(result)) {
    gab_result_dump_error(result);
  }

  gab_result_destroy(result);
  s_u8_destroy(src);
  gab_destroy(gab);
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
