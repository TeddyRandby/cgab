#include "builtins.h"
#include "include/gab.h"
#include "include/import.h"
#include "include/os.h"
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

typedef gab_value (*handler_f)(gab_engine *, gab_vm *, a_i8 *path,
                               const s_i8 module);

typedef gab_value (*module_f)(gab_engine *, gab_vm *);

typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

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

  gab_value res = symbol(gab, vm);

  gab_imports_shared(gab, s_i8_create(path->data, path->len), handle, res);

  return res;
}

gab_value gab_source_file_handler(gab_engine *gab, gab_vm *vm, a_i8 *path,
                                  const s_i8 module) {
  a_i8 *src = os_read_file((char *)path->data);

  gab_value pkg =
      gab_compile(gab,
                  GAB_VAL_OBJ(gab_obj_string_create(
                      gab, s_i8_create(path->data, path->len))),
                  s_i8_create(src->data, src->len), fGAB_DUMP_ERROR);

  a_i8_destroy(src);

  if (GAB_VAL_IS_UNDEFINED(pkg))
    return gab_panic(gab, vm, "Failed to compile module");

  a_gab_value *res = gab_run(gab, pkg, fGAB_DUMP_ERROR);

  if (res == NULL)
      return GAB_VAL_UNDEFINED();

  gab_imports_module(gab, s_i8_create(path->data, path->len), pkg,
                     res->data[0]);

  gab_value val = res->data[0];

  a_gab_value_destroy(res);

  return val;
}

resource resources[] = {
    // Local resources
    {.prefix = "./mod/", .suffix = ".gab", .handler = gab_source_file_handler},
    {.prefix = "./",
     .suffix = "/mod/mod.gab",
     .handler = gab_source_file_handler},
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
    {.prefix = "/usr/local/share/gab/",
     .suffix = "mod/mod.gab",
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

void gab_lib_require(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {

  if (!GAB_VAL_IS_STRING(argv[0]) || argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_require");
  }

  gab_obj_string *arg = GAB_VAL_TO_STRING(argv[0]);

  const s_i8 name = gab_obj_string_ref(arg);

  for (i32 i = 0; i < sizeof(resources) / sizeof(resource); i++) {
    resource *res = resources + i;
    a_i8 *path = match_resource(res, name);

    if (path) {
      gab_value cached =
          gab_imports_exists(gab, s_i8_create(path->data, path->len));

      if (!GAB_VAL_IS_UNDEFINED(cached)) {
        gab_push(vm, 1, &cached);
        a_i8_destroy(path);
        return;
      }

      gab_value result = res->handler(gab, vm, path, name);
      a_i8_destroy(path);
      gab_push(vm, 1, &result);
      return;
    }

    a_i8_destroy(path);
  }

  gab_panic(gab, vm, "Could not locate module");
}

void gab_lib_panic(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc == 1) {
    gab_obj_string *str = GAB_VAL_TO_STRING(gab_val_to_s(gab, argv[0]));
    char buffer[str->len + 1];
    memcpy(buffer, str->data, str->len);
    buffer[str->len] = '\0';

    gab_panic(gab, vm, buffer);
    return;
  }

  gab_panic(gab, vm, "Error");
}

void gab_lib_print(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  for (u8 i = 0; i < argc; i++) {
    if (i > 0)
      putc(' ', stdout);
    gab_val_dump(stdout, argv[i]);
  }

  printf("\n");
}

void gab_setup_builtins(gab_engine *gab) {

  gab_value require = GAB_BUILTIN(require);
  gab_value panic = GAB_BUILTIN(panic);
  gab_value print = GAB_BUILTIN(print);

  gab_specialize(gab, NULL, GAB_STRING("require"), gab_type(gab, kGAB_STRING),
                 require);

  gab_specialize(gab, NULL, GAB_STRING("panic"), gab_type(gab, kGAB_STRING),
                 panic);

  gab_specialize(gab, NULL, GAB_STRING("print"), gab_type(gab, kGAB_UNDEFINED),
                 print);
}
