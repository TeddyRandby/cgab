#include "builtins.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/os.h"
#include "include/types.h"
#include "include/value.h"
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

typedef gab_value (*handler_f)(gab_engine *, gab_vm *, a_i8 *path,
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
    gab_module *mod;
    void *shared;
  } as;
  gab_value cache;
};

d_import imports = {0};

void import_destroy(gab_engine *gab, import *i) {
  switch (i->k) {
  case IMPORT_SHARED:
    dlclose(i->as.shared);
  default:
    break;
  }
  DESTROY(i);
}

void imports_create() { d_import_create(&imports, 8); };

void imports_destroy(gab_engine *gab) {
  for (u64 i = 0; i < imports.cap; i++) {
    if (d_import_iexists(&imports, i)) {
      import_destroy(gab, d_import_ival(&imports, i));
    }
  }

  d_import_destroy(&imports);
};

void add_import(gab_engine *gab, s_i8 name, import *i) {
  u64 hash = s_i8_hash(name, gab->hash_seed);
  d_import_insert(&imports, hash, i);
}

gab_value check_import(gab_engine *gab, s_i8 name) {
  u64 hash = s_i8_hash(name, gab->hash_seed);

  if (d_import_exists(&imports, hash)) {
    import *i = d_import_read(&imports, hash);
    return i->cache;
  }

  return GAB_VAL_UNDEFINED();
}

gab_value gab_shared_object_handler(gab_engine *gab, gab_vm *vm, a_i8 *path,
                                    const s_i8 module) {

  void *handle = dlopen((char *)path->data, RTLD_LAZY);

  if (!handle) {
    gab_panic(gab, vm, "Couldn't open module");
  }

  module_f symbol = dlsym(handle, "gab_mod");

  if (!symbol) {
    dlclose(handle);
    gab_panic(gab, vm, "Missing symbol 'gab_mod'");
  }

  gab_value res = symbol(gab);

  import *i = NEW(import);

  i->k = IMPORT_SHARED;
  i->cache = res;
  i->as.shared = handle;

  add_import(gab, module, i);

  gab_dref(gab, vm, res);

  return res;
}

gab_value gab_source_file_handler(gab_engine *gab, gab_vm *vm, a_i8 *path,
                                  const s_i8 module) {
  a_i8 *src = os_read_file((char *)path->data);

  gab_module *pkg =
      gab_compile(gab, s_i8_create(path->data, path->len),
                  s_i8_create(src->data, src->len), GAB_FLAG_DUMP_ERROR);

  a_i8_destroy(src);

  if (pkg == NULL) {
    return gab_panic(gab, vm, "Failed to compile module");
  }

  gab_value res =
      gab_run(gab, pkg, GAB_FLAG_DUMP_ERROR);

  import *i = NEW(import);

  i->k = IMPORT_SOURCE;
  i->cache = res;
  i->as.mod = pkg;

  add_import(gab, module, i);

  gab_dref(gab, vm, res);

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
    {.prefix = "/usr/local/share/gab/",
     .suffix = ".gab",
     .handler = gab_source_file_handler},
    {.prefix = "/usr/local/share/gab/std/",
     .suffix = ".gab",
     .handler = gab_source_file_handler},
    {.prefix = "/usr/local/share/gab/",
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

gab_value gab_lib_require(gab_engine *gab, gab_vm *vm, u8 argc,
                          gab_value argv[argc]) {

  if (!GAB_VAL_IS_STRING(argv[0]) || argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_require");
  }

  gab_obj_string *arg = GAB_VAL_TO_STRING(argv[0]);
  const s_i8 module = gab_obj_string_ref(arg);

  gab_value cached = check_import(gab, module);
  if (!GAB_VAL_IS_UNDEFINED(cached)) {
    return cached;
  }

  for (i32 i = 0; i < sizeof(resources) / sizeof(resource); i++) {
    resource *res = resources + i;
    a_i8 *path = match_resource(res, module);

    if (path) {
      gab_value result = res->handler(gab, vm, path, module);
      a_i8_destroy(path);
      return result;
    }

    a_i8_destroy(path);
  }

  gab_panic(gab, vm, "Could not locate module");
}
