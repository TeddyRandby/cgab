#include "../../common/os.h"
#include "lib.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef gab_value (*handler_f)(gab_engine *, const s_u8 *path);

typedef struct {
  handler_f handler;
  const char *prefix;
  const char *suffix;
} resource;

gab_value gab_source_file_handler(gab_engine *eng, const s_u8 *path) {
  gab_engine *fork = gab_create_fork(eng);

  s_u8 *src = os_read_file((char *)path->data);

  gab_result *result = gab_run_source(fork, "__main__", src, GAB_FLAG_NONE);

  if (gab_result_has_error(result)) {
    gab_result_dump_error(result);

    gab_result_destroy(result);
    gab_destroy_fork(fork);
    return GAB_VAL_NULL();
  }

  gab_value val = result->as.result;

  gab_result_destroy(result);
  gab_destroy_fork(fork);

  return val;
}

resource resources[] = {
    {.prefix = "./", .suffix = ".gab", .handler = gab_source_file_handler},
    {.prefix = "./", .suffix = "/mod.gab", .handler = gab_source_file_handler}};

s_u8 *match_resource(resource *res, s_u8_ref name) {
  const u64 p_len = strlen(res->prefix);
  const u64 s_len = strlen(res->suffix);

  s_u8 *buffer = s_u8_create_empty(p_len + name.size + s_len + 1);

  memcpy(buffer->data, res->prefix, p_len);
  memcpy(buffer->data + p_len, name.data, name.size);
  memcpy(buffer->data + p_len + name.size, res->suffix, s_len + 1);

  if (access((char *)buffer->data, F_OK) == 0) {
    return buffer;
  }

  s_u8_destroy(buffer);

  return NULL;
}

gab_value gab_lib_require(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *arg = GAB_VAL_TO_STRING(argv[0]);

  for (i32 i = 0; i < sizeof(resources) / sizeof(resource); i++) {
    resource *res = resources + i;
    s_u8 *path = match_resource(res, gab_obj_string_ref(arg));

    if (path) {
      gab_value result = res->handler(eng, path);
      s_u8_destroy(path);
      return result;
    }

    s_u8_destroy(path);
  }

  return GAB_VAL_NULL();
}
