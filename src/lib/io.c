#include "../gab/gab.h"
#include "src/core/core.h"
#include "src/gab/engine.h"
#include "src/gab/object.h"
#include "src/gab/value.h"
#include <string.h>

gab_value gab_lib_open(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 1) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);
  char *cpath = (char *)path->data;

  FILE *file = fopen(cpath, "rb");
  if (file == NULL) {
    return GAB_VAL_NULL();
  }

  gab_obj_container *container =
      gab_obj_container_create(eng, GAB_VAL_NULL(), file);

  gab_engine_obj_dref(eng, (gab_obj *)container);

  return GAB_VAL_OBJ(container);
}

gab_value gab_lib_read(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 1) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);
  char *cpath = (char *)path->data;

  FILE *file = fopen(cpath, "rb");
  if (file == NULL) {
    return GAB_VAL_NULL();
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = NEW_ARRAY(char, fileSize + 1);
  if (buffer == NULL) {
    fclose(file);
    DESTROY(buffer);
    exit(1);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fclose(file);
    DESTROY(buffer);
    return GAB_VAL_NULL();
  }

  buffer[bytesRead] = '\0';

  fclose(file);

  gab_obj_string *result = gab_obj_string_create(eng, s_i8_cstr(buffer));

  DESTROY(buffer);
  return GAB_VAL_OBJ(result);
}

gab_value gab_lib_write(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 2) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[1])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);
  char *cpath = (char *)path->data;
  gab_obj_string *data = GAB_VAL_TO_STRING(argv[1]);
  char *cdata = (char *)data->data;
  FILE *file = fopen(cpath, "w");
  if (file == NULL) {
    return GAB_VAL_NULL();
  }

  i32 result = fputs(cdata, file);
  fclose(file);

  if (result > 0) {
    return GAB_VAL_BOOLEAN(true);
  } else {
    return GAB_VAL_NULL();
  }
}

void gab_container_file_cb(gab_engine *eng, gab_obj_container *self) {
  printf("Calling cb\n");
  if (!fclose(self->data)) {
    fprintf(stderr, "Uh oh, file close failed.");
    exit(1);
  }
  printf("Closed file\n");
}

gab_value gab_mod(gab_engine *gab) {
  // Register the file container
  gab_engine_add_container_tag(gab, GAB_VAL_NULL(), gab_container_file_cb);

  gab_lib_kvp io_kvps[] = {GAB_KVP_BUILTIN(open, 1), GAB_KVP_BUILTIN(read, 1),
                           GAB_KVP_BUILTIN(write, 2)};

  return gab_bundle_kvps(gab, GAB_KVP_BUNDLESIZE(io_kvps), io_kvps);
}
