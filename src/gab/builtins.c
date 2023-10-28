#include "include/builtins.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/os.h"
#include <dlfcn.h>
#include <stdio.h>

#define MODULE_SYMBOL "gab_lib"

typedef a_gab_value *(*handler_f)(struct gab_triple, const char *);

typedef a_gab_value *(*module_f)(struct gab_triple);

typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

a_gab_value *gab_shared_object_handler(struct gab_triple gab,
                                       const char *path) {
  void *handle = dlopen(path, RTLD_LAZY);

  if (!handle) {
    gab_panic(gab, "Couldn't open module");
    return NULL;
  }

  module_f symbol = dlsym(handle, MODULE_SYMBOL);

  if (!symbol) {
    dlclose(handle);
    gab_panic(gab, "Missing symbol " MODULE_SYMBOL);
    return NULL;
  }

  a_gab_value *res = symbol(gab);

  gab_impputshd(gab.eg, path, handle, res);

  return res;
}

a_gab_value *gab_source_file_handler(struct gab_triple gab, const char *path) {
  a_char *src = os_read_file(path);

  if (src == NULL)
    return a_gab_value_one(gab_panic(gab, "Failed to read module"));

  gab_value pkg =
      gab_cmpl(gab, (struct gab_cmpl_argt){
                        .name = path,
                        .source = (const char *)src->data,
                        .flags = fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC,
                    });

  a_char_destroy(src);

  a_gab_value *res =
      gab_run(gab, (struct gab_run_argt){
                       .main = pkg,
                       .flags = fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC,
                   });

  gab_impputmod(gab.eg, path, pkg, res);
  gab_negkeep(gab.eg, res->len, res->data);

  return res;
}

resource resources[] = {
    // Local resources
    {
        .prefix = "./mod/",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./",
        .suffix = "/mod/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./",
        .suffix = "/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "./libcgab",
        .suffix = ".so",
        .handler = gab_shared_object_handler,
    },
    // Installed resources
    {
        .prefix = "/usr/local/share/gab/",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "/usr/local/share/gab/",
        .suffix = "/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "/usr/local/share/gab/",
        .suffix = "mod/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = "/usr/local/lib/gab/libcgab",
        .suffix = ".so",
        .handler = gab_shared_object_handler,
    },
};

a_char *match_resource(resource *res, const char *name, uint64_t len) {
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
  return a_char_create(buffer, total_len);
}

void gab_lib_use(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc == 1) {
    gab_panic(gab, "Invalid call to gab_lib_require");
    return;
  }

  // skip first argument
  for (size_t i = 1; i < argc; i++) {
    s_char name = gab_valintocs(gab, argv[i]);

    for (int j = 0; j < sizeof(resources) / sizeof(resource); j++) {
      resource *res = resources + j;
      a_char *path = match_resource(res, name.data, name.len);

      if (path) {
        struct gab_imp *cached = gab_impat(gab.eg, (char *)path->data);

        if (cached != NULL) {
          a_gab_value *v = gab_impval(cached);
          if (v != NULL)
            gab_nvmpush(gab.vm, v->len, v->data);
          goto fin;
        }

        a_gab_value *result = res->handler(gab, (char *)path->data);

        if (result != NULL) {
          gab_nvmpush(gab.vm, result->len, result->data);
          goto fin;
        }

      fin:
        a_char_destroy(path);
        return;
      }
    }
  }

  gab_panic(gab, "Could not locate module");
}

void gab_lib_panic(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc == 1) {
    s_char str = gab_valintocs(gab, argv[0]);

    char *cstr = strndup((char *)str.data, str.len);

    gab_panic(gab, cstr);

    free(cstr);

    return;
  }

  gab_panic(gab, "Error");
}

void gab_lib_print(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  for (uint8_t i = 0; i < argc; i++) {
    if (i > 0)
      putc(' ', stdout);
    gab_fdump(stdout, argv[i]);
  }

  printf("\n");
}

void gab_setup_builtins(struct gab_triple gab) {
  gab_spec(gab, (struct gab_spec_argt){
                    .name = "use",
                    .receiver = gab_typ(gab.eg, kGAB_NIL),
                    .specialization = gab_sbuiltin(gab, "use", gab_lib_use),
                });

  gab_spec(gab,
           (struct gab_spec_argt){
               .name = "panic",
               .receiver = gab_typ(gab.eg, kGAB_STRING),
               .specialization = gab_sbuiltin(gab, "panic", gab_lib_panic),
           });

  gab_spec(gab,
           (struct gab_spec_argt){
               .name = "print",
               .receiver = gab_typ(gab.eg, kGAB_UNDEFINED),
               .specialization = gab_sbuiltin(gab, "print", gab_lib_print),
           });
}
