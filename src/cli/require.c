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

typedef struct {
  import_k k;
  union {
    a_i8 *source;
    void *shared;
  } as;
  gab_value cache;
} import;

void import_destroy_cb(gab_obj_container *container) {
  import *i = container->data;

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

void add_import(gab_engine *gab, s_i8 name, import *i) {
  if (!GAB_VAL_IS_RECORD(gab->std)) {
    return;
  }

  gab_value mod_prop =
      GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr("__mod__")));

  gab_value mod = gab_obj_record_read(GAB_VAL_TO_RECORD(gab->std), mod_prop);

  if (!GAB_VAL_IS_RECORD(mod)) {
    return;
  }

  gab_value container =
      GAB_VAL_OBJ(gab_obj_container_create(import_destroy_cb, i));

  gab_value import_prop = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

  gab_obj_record_insert(gab, GAB_VAL_TO_RECORD(mod), import_prop, container);
}

gab_value check_import(gab_engine *gab, s_i8 name) {
  // Check that we have a std object.
  if (!GAB_VAL_IS_RECORD(gab->std)) {
    return GAB_VAL_NULL();
  }

  gab_value mod_prop =
      GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_cstr("__mod__")));

  gab_value mod = gab_obj_record_read(GAB_VAL_TO_RECORD(gab->std), mod_prop);

  // Check that we have a mod property as we expect.
  if (!GAB_VAL_IS_RECORD(mod)) {
    return GAB_VAL_NULL();
  }

  gab_value import_prop = GAB_VAL_OBJ(gab_obj_string_create(gab, name));

  gab_value i = gab_obj_record_read(GAB_VAL_TO_RECORD(mod), import_prop);

  // Check that the property off of mod is as we expect.
  if (!GAB_VAL_IS_CONTAINER(i)) {
    return GAB_VAL_NULL();
  }

  gab_obj_container *container = GAB_VAL_TO_CONTAINER(i);

  // If the destructors match, we can reasonably expect that the
  // container is the type we hope it is.
  if (container->destructor != import_destroy_cb) {
    return GAB_VAL_NULL();
  }

  return ((import *)container->data)->cache;
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

  gab_value pkg = gab_compile(gab, s_i8_create(path->data, path->len),
                              s_i8_create(src->data, src->len));

  if (GAB_VAL_IS_NULL(pkg)) {
    return GAB_VAL_NULL();
  }

  gab_value res = gab_run(gab, pkg);

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

gab_value gab_lib_require(gab_engine *gab, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *arg = GAB_VAL_TO_STRING(argv[0]);
  const s_i8 module = gab_obj_string_ref(arg);

  gab_value cached = check_import(gab, module);
  if (!GAB_VAL_IS_NULL(cached)) {
    // Because the result of a builtin is always decremented,
    // increment the cached values when they are returned.
    gab_iref(gab, cached);
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
