#include "../core/os.h"
#include "../gab/gab.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef gab_value (*handler_f)(gab_engine *, const a_i8 *path,
                               const s_i8 module);

typedef gab_value (*module_f)(gab_engine *);


typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

gab_value gab_shared_object_handler(gab_engine *eng, const a_i8 *path,
                                    const s_i8 module) {

  void *handle = dlopen((char *)path->data, RTLD_LAZY);

  if (!handle)
    return GAB_VAL_NULL();

  module_f symbol = dlsym(handle, "gab_mod");

  if (!symbol) {
    dlclose(handle);
    return GAB_VAL_NULL();
  }

  gab_value val = symbol(eng);

  gab_import *import = gab_import_shared(handle, val);

  gab_engine_add_import(eng, import, module);

  return val;
}

gab_value gab_source_file_handler(gab_engine *eng, const a_i8 *path,
                                  const s_i8 module) {
  gab_engine *fork = gab_create_fork(eng);

  a_i8 *src = os_read_file((char *)path->data);

  gab_result *result = gab_run_source(
      fork, "__main__", s_i8_create(src->data, src->len), GAB_FLAG_NONE);

  if (gab_result_has_error(result)) {
    gab_result_dump_error(result);

    gab_result_destroy(result);
    gab_destroy(fork);
    return GAB_VAL_NULL();
  }

  gab_value val = result->as.result;

  gab_import *import = gab_import_source(src, val);

  gab_engine_add_import(eng, import, module);

  gab_result_destroy(result);
  gab_destroy(fork);

  return val;
}

resource resources[] = {
    // Local resources
    {.prefix = "./", .suffix = ".gab", .handler = gab_source_file_handler},
    {.prefix = "./", .suffix = "/mod.gab", .handler = gab_source_file_handler},
    {.prefix = "./libcgab",
     .suffix = ".so",
     .handler = gab_shared_object_handler},
    // Installed resources
    {.prefix = "/usr/local/lib/gab/",
     .suffix = ".gab",
     .handler = gab_source_file_handler},
    {.prefix = "/usr/local/lib/gab/",
     .suffix = "/mod.gab",
     .handler = gab_source_file_handler},
    {.prefix = "/usr/local/lib/gab/libcgab",
     .suffix = ".so",
     .handler = gab_shared_object_handler},
};

a_i8 *match_resource(resource *res, s_i8 name) {
  const u64 p_len = strlen(res->prefix);
  const u64 s_len = strlen(res->suffix);

  a_i8 *buffer = a_i8_empty(p_len + name.len + s_len + 1);

  memcpy(buffer->data, res->prefix, p_len);
  memcpy(buffer->data + p_len, name.data, name.len);
  memcpy(buffer->data + p_len + name.len, res->suffix, s_len + 1);

  if (access((char *)buffer->data, F_OK) == 0) {
    return buffer;
  }

  a_i8_destroy(buffer);

  return NULL;
}

gab_value gab_lib_require(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *arg = GAB_VAL_TO_STRING(argv[0]);
  const s_i8 module = gab_obj_string_ref(arg);

  for (i32 i = 0; i < sizeof(resources) / sizeof(resource); i++) {
    resource *res = resources + i;
    a_i8 *path = match_resource(res, module);

    if (path) {
      gab_value result = res->handler(eng, path, module);
      a_i8_destroy(path);
      return result;
    }

    a_i8_destroy(path);
  }

  return GAB_VAL_NULL();
}

static inline void print_values(gab_value *argv, u8 argc) {
  for (i32 i = 0; i < argc; i++) {
    gab_value arg = argv[i];
    gab_val_dump(arg);

    if (i == (argc - 1)) {
      // last iteration
      printf("\n");
    } else {
      printf(", ");
    }
  }
}

gab_value gab_lib_print(gab_engine *eng, gab_value *argv, u8 argc) {
  print_values(argv, argc);
  return GAB_VAL_NULL();
}

a_i8 *collect_line() {
  printf(">>>");
  return os_read_line();
}

void bind_std(gab_engine *gab) {

  gab_lib_kvp builtins[] = {
      GAB_BUILTIN(print, VAR_RET),
      GAB_BUILTIN(require, 1),
  };

  gab_bind_library(gab, GAB_BUNDLESIZE(builtins), builtins);
}

void gab_repl() {

  gab_engine *gab = gab_create();
  bind_std(gab);

  printf("Welcome to Gab v%d.%d\nPress CTRL+D to exit\n", GAB_VERSION_MAJOR,
         GAB_VERSION_MINOR);

  while (true) {

    a_i8 *src = collect_line();

    if (src == NULL) {
      break;
    }

    if (src->len > 1 && src->data[0] != '\n') {
      gab_result *result = gab_run_source(
          gab, "__repl__", s_i8_create(src->data, src->len), GAB_FLAG_NONE);

      if (gab_result_has_error(result)) {
        gab_result_dump_error(result);
      } else {
        gab_val_dump(result->as.result);
        printf("\n");
      }

      gab_result_destroy(result);
    }
  }

  gab_destroy(gab);
}

void gab_run_file(const char *path) {
  a_i8 *src = os_read_file(path);
  gab_engine *gab = gab_create();
  bind_std(gab);

  gab_result *result = gab_run_source(
      gab, "__main__", s_i8_create(src->data, src->len), GAB_FLAG_NONE);

  if (gab_result_has_error(result)) {
    gab_result_dump_error(result);
  }

  gab_result_destroy(result);
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
