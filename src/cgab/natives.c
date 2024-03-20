#include "natives.h"
#include "core.h"
#include "gab.h"
#include "os.h"
#include <dlfcn.h>
#include <stdio.h>

#if OS_UNIX
#include <unistd.h>
#endif

void *osdlopen(const char *path) {
#if OS_UNIX
  return dlopen(path, RTLD_NOW);
#else
#error Windows not supported
#endif
}

void *osdlsym(void *handle, const char *path) {
#if OS_UNIX
  return dlsym(handle, path);
#else
#error Windows not supported
#endif
}

void osdlclose(void *handle) {
#if OS_UNIX
  dlclose(handle);
#else
#error Windows not supported
#endif
}

#define BUFFER_MAX 1024
a_char *oscwd() {
#if OS_UNIX
  a_char *result = a_char_empty(BUFFER_MAX);
  getcwd((char *)result->data, BUFFER_MAX);

  return result;
#endif
}

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
  void *handle = osdlopen(path);

  if (!handle) {
    gab_panic(gab, "Couldn't open module");
    return NULL;
  }

  module_f symbol = osdlsym(handle, MODULE_SYMBOL);

  if (!symbol) {
    osdlclose(handle);
    gab_panic(gab, "Missing symbol " MODULE_SYMBOL);
    return NULL;
  }

  gab_gclock(gab.gc);
  a_gab_value *res = symbol(gab);
  gab_gcunlock(gab.gc);

  if (res) {
    a_gab_value *final =
        gab_segmodput(gab.eg, path, gab_nil, res->len, res->data);

    free(res);

    return final;
  }

  return gab_segmodput(gab.eg, path, gab_nil, 0, NULL);
}

a_gab_value *gab_source_file_handler(struct gab_triple gab, const char *path) {
  a_char *src = gab_osread(path);

  if (src == NULL)
    return gab_panic(gab, "Failed to load module");

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

  if (res->data[0] != gab_string(gab, "ok"))
    return gab_panic(gab, "Failed to load module");

  gab_negkeep(gab.eg, res->len - 1, res->data + 1);

  a_gab_value *final = gab_segmodput(gab.eg, path, pkg, res->len - 1, res->data + 1);

  free(res);

  return final;
}

#ifndef GAB_PREFIX
#define GAB_PREFIX "."
#endif

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
        .prefix = GAB_PREFIX "/gab/modules/",
        .suffix = ".gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = GAB_PREFIX "/gab/modules/",
        .suffix = "/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = GAB_PREFIX "/gab/modules/",
        .suffix = "/mod/mod.gab",
        .handler = gab_source_file_handler,
    },
    {
        .prefix = GAB_PREFIX "/gab/modules/libcgab",
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

a_gab_value *gab_lib_use(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {

  gab_value mod = gab_arg(0);

  if (gab_valkind(mod) != kGAB_STRING)
    return gab_ptypemismatch(gab, mod, gab_type(gab.eg, kGAB_STRING));

  const char *name = gab_valintocs(gab, mod);

  for (int j = 0; j < sizeof(resources) / sizeof(resource); j++) {
    resource *res = resources + j;
    a_char *path = match_resource(res, name, strlen(name));

    if (path) {
      a_gab_value *cached = gab_segmodat(gab.eg, (char *)path->data);

      if (cached != NULL) {
        /* SKip the first argument, which is the module's data */
        gab_nvmpush(gab.vm, cached->len - 1, cached->data + 1);
        a_char_destroy(path);
        return NULL;
      }

      a_gab_value *result = res->handler(gab, (char *)path->data);

      if (result != NULL) {
        /* SKip the first argument, which is the module's data */
        gab_nvmpush(gab.vm, result->len - 1, result->data + 1);
        a_char_destroy(path);

        return NULL;
      }
    }
  }

  return gab_panic(gab, "Could not locate module:\n\n | $", mod);
}

a_gab_value *gab_lib_panic(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  if (argc == 1) {
    const char *str = gab_valintocs(gab, argv[0]);

    return gab_panic(gab, str);
  }

  return gab_panic(gab, "Error");
}

a_gab_value *gab_lib_print(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  int start = argv[0] == gab_undefined;

  for (int i = start; i < argc; i++) {

    if (i > start)
      fputs(", ", stdout);

    gab_fvalinspect(stdout, argv[i], -1);
  }

  printf("\n");

  return NULL;
}

void gab_setup_natives(struct gab_triple gab) {
  gab_egkeep(
      gab.eg,
      gab_iref(gab,
               gab_spec(gab, (struct gab_spec_argt){
                                 .name = "use",
                                 .receiver = gab_type(gab.eg, kGAB_STRING),
                                 .specialization =
                                     gab_snative(gab, "use", gab_lib_use),
                             })));

  gab_egkeep(
      gab.eg,
      gab_iref(gab, gab_spec(gab, (struct gab_spec_argt){
                                      .name = "panic",
                                      .receiver = gab_type(gab.eg, kGAB_STRING),
                                      .specialization = gab_snative(
                                          gab, "panic", gab_lib_panic),
                                  })));

  gab_egkeep(
      gab.eg,
      gab_iref(gab,
               gab_spec(gab, (struct gab_spec_argt){
                                 .name = "print",
                                 .receiver = gab_type(gab.eg, kGAB_UNDEFINED),
                                 .specialization =
                                     gab_snative(gab, "print", gab_lib_print),
                             })));
}
