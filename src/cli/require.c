#include "require.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/os.h"
#include "include/types.h"
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

typedef gab_value (*handler_f)(gab_engine *, const a_i8 *path,
                               const s_i8 module);

typedef gab_value (*module_f)(gab_engine *);

typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

typedef enum {
  IMPORT_SHARED,
  IMPORT_SOURCE,
} import_k;

struct import {
  import_k k;
  union {
    a_i8 *source;
    void *shared;
  } as;
  gab_value cache;
};

d_import imports = {0};
extern gab_value globals;

void import_destroy(import *i) {
  switch (i->k) {
  case IMPORT_SHARED:
    dlclose(i->as.shared);
    break;
  case IMPORT_SOURCE:
    a_i8_destroy(i->as.source);
    break;
  }
  DESTROY(i);
}

void imports_create() { d_import_create(&imports, 8); };

void imports_destroy() {
  for (u64 i = 0; i < imports.cap; i++) {
    if (d_import_iexists(&imports, i)) {
      import_destroy(d_import_ival(&imports, i));
    }
  }

  d_import_destroy(&imports);
};

void add_import(gab_engine *gab, s_i8 name, import *i) {
  d_import_insert(&imports, name, i);
}

gab_value check_import(gab_engine *gab, s_i8 name) {
  if (d_import_exists(&imports, name)) {
    import *i = d_import_read(&imports, name);
    return i->cache;
  }
  return GAB_VAL_NULL();
}

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

  gab_value result = symbol(eng);

  import *i = NEW(import);

  i->k = IMPORT_SHARED;
  i->cache = result;
  i->as.shared = handle;

  add_import(eng, module, i);

  return result;
}

gab_value gab_source_file_handler(gab_engine *gab, const a_i8 *path,
                                  const s_i8 module) {
  a_i8 *src = os_read_file((char *)path->data);

  gab_module *pkg = gab_compile_main(gab, s_i8_create(src->data, src->len));

  if (pkg == NULL) {
    return GAB_VAL_NULL();
  }

  i32 vm = gab_spawn(gab);

  gab_value res = gab_run_main(gab, vm, pkg, globals);

  import *i = NEW(import);

  i->k = IMPORT_SOURCE;
  i->cache = res;
  i->as.source = src;
  add_import(gab, module, i);

  return res;
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

gab_value gab_lib_require(gab_engine *gab, i32 vm, u8 argc,
                          gab_value argv[argc]) {

  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *arg = GAB_VAL_TO_STRING(argv[0]);
  const s_i8 module = gab_obj_string_ref(arg);

  gab_value cached = check_import(gab, module);
  if (!GAB_VAL_IS_NULL(cached)) {
    // Because the result of a builtin is always decremented,
    // increment the cached values when they are returned.
    gab_iref(gab, vm, cached);
    return cached;
  }

  for (i32 i = 0; i < sizeof(resources) / sizeof(resource); i++) {
    resource *res = resources + i;
    a_i8 *path = match_resource(res, module);

    if (path) {
      gab_value result = res->handler(gab, path, module);
      a_i8_destroy(path);
      return result;
    }

    a_i8_destroy(path);
  }

  return GAB_VAL_NULL();
}
