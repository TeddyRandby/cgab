#include "require.h"
#include "../core/os.h"
#include "src/gab/engine.h"
#include <dlfcn.h>
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

  gab_add_import(eng, module, import);

  gab_dref(eng, val);
  return val;
}

gab_value gab_source_file_handler(gab_engine *gab, const a_i8 *path,
                                  const s_i8 module) {
  gab_engine *fork = gab_fork(gab);

  a_i8 *src = os_read_file((char *)path->data);

  gab_result result = gab_run(fork, s_i8_cstr("__main__"),
                              s_i8_create(src->data, src->len), GAB_FLAG_NONE);

  if (!gab_result_ok(result)) {
    fprintf(stderr, "%.*s", (i32)gab->error->len, gab->error->data);

    gab_destroy(fork);
    return GAB_VAL_NULL();
  }

  gab_value val = gab_result_value(result);

  gab_import *import = gab_import_source(src, val);

  gab_add_import(gab, module, import);

  gab_destroy(fork);

  gab_dref(gab, val);
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
    {.prefix = "/usr/local/lib/",
     .suffix = ".gab",
     .handler = gab_source_file_handler},
    {.prefix = "/usr/local/lib/",
     .suffix = "/mod.gab",
     .handler = gab_source_file_handler},
    {.prefix = "/usr/local/lib/libcgab",
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
