#include "include/builtins.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/import.h"
#include "include/os.h"
#include <dlfcn.h>
#include <stdio.h>

#define MODULE_SYMBOL "gab_lib"

typedef a_gab_value *(*handler_f)(gab_eg *, gab_vm *, const char *,
                                  const char *);

typedef a_gab_value *(*module_f)(gab_eg *, gab_vm *);

typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

a_gab_value *gab_shared_object_handler(gab_eg *gab, gab_vm *vm,
                                       const char *path, const char *name) {

  void *handle = dlopen(path, RTLD_LAZY);

  if (!handle) {
    gab_panic(gab, vm, "Couldn't open module");
    return NULL;
  }

  module_f symbol = dlsym(handle, MODULE_SYMBOL);

  if (!symbol) {
    dlclose(handle);
    gab_panic(gab, vm, "Missing symbol " MODULE_SYMBOL);
    return NULL;
  }

  a_gab_value *res = symbol(gab, vm);

  gab_impshd(gab, path, handle, res);

  return res;
}

a_gab_value *gab_source_file_handler(gab_eg *gab, gab_vm *vm, const char *path,
                                     const char *module) {
  a_int8_t *src = os_read_file(path);

  if (src == NULL)
    return a_gab_value_one(gab_panic(gab, vm, "Failed to read module"));

  gab_value pkg =
      gab_block(gab, (struct gab_block_argt){
                         .name = module,
                         .source = (const char *)src->data,
                         .flags = fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC,
                     });

  a_int8_t_destroy(src);

  a_gab_value *res = gab_run(gab, pkg, fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC);

  gab_impmod(gab, path, pkg, res);

  return res;
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

a_int8_t *match_resource(resource *res, const char *name, uint64_t len) {
  const uint64_t p_len = strlen(res->prefix);
  const uint64_t s_len = strlen(res->suffix);
  const uint64_t total_len = p_len + len + s_len + 1;

  char buffer[total_len];

  memcpy(buffer, res->prefix, p_len);
  memcpy(buffer + p_len, name, len);
  memcpy(buffer + p_len + len, res->suffix, s_len + 1);

  FILE *f = fopen(buffer, "r");

  if (!f)
    return NULL;

  fclose(f);
  return a_int8_t_create((int8_t *)buffer, total_len);
}

void gab_lib_require(gab_eg *gab, gab_vm *vm, size_t argc,
                     gab_value argv[argc]) {

  if (argc != 1)
    gab_panic(gab, vm, "Invalid call to gab_lib_require");

  char *name = gab_valtocs(gab, argv[0]);

  for (int32_t i = 0; i < sizeof(resources) / sizeof(resource); i++) {
    resource *res = resources + i;
    a_int8_t *path = match_resource(res, name, strlen(name));

    if (path) {
      a_gab_value *cached = gab_imphas(gab, (char *)path->data);

      if (cached != NULL) {
        gab_nvmpush(vm, cached->len, cached->data);
        goto fin;
      }

      a_gab_value *result = res->handler(gab, vm, (char *)path->data, name);
      free(name);

      if (result != NULL) {
        gab_nvmpush(vm, result->len, result->data);
        goto fin;
      }

    fin:
      a_int8_t_destroy(path);
      return;
    }
  }

  gab_panic(gab, vm, "Could not locate module");
}

void gab_lib_panic(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc == 1) {
    char *str = gab_valtocs(gab, argv[0]);

    gab_panic(gab, vm, str);

    free(str);
    return;
  }

  gab_panic(gab, vm, "Error");
}

void gab_lib_print(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  for (uint8_t i = 0; i < argc; i++) {
    if (i > 0)
      putc(' ', stdout);
    gab_fdump(stdout, argv[i]);
  }

  printf("\n");
}

void gab_setup_builtins(gab_eg *gab) {
  gab_spec(gab, NULL,
           (struct gab_spec_argt){
               .name = "require",
               .receiver = gab_typ(gab, kGAB_STRING),
               .specialization = gab_builtin(gab, "require", gab_lib_require),
          });

  gab_spec(gab, NULL,
           (struct gab_spec_argt){
               .name = "panic",
               .receiver = gab_typ(gab, kGAB_STRING),
               .specialization = gab_builtin(gab, "panic", gab_lib_panic),
           });

  gab_spec(gab, NULL,
           (struct gab_spec_argt){
               .name = "print",
               .receiver = gab_typ(gab, kGAB_UNDEFINED),
               .specialization = gab_builtin(gab, "print", gab_lib_print),
           });
}
