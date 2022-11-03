#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>

void gab_container_file_cb(gab_obj_container *self) {
  if (fclose(self->data)) {
    fprintf(stderr, "Uh oh, file close failed.");
    exit(1);
  }
}

gab_value gab_lib_open(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(argv[0]) || !GAB_VAL_IS_STRING(argv[1])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *path_obj = GAB_VAL_TO_STRING(argv[0]);
  char path[path_obj->len + 1];
  memcpy(path, path_obj->data, sizeof(i8) * path_obj->len);
  path[path_obj->len] = '\0';

  gab_obj_string *perms_obj = GAB_VAL_TO_STRING(argv[1]);
  char perms[perms_obj->len + 1];
  memcpy(perms, perms_obj->data, sizeof(i8) * perms_obj->len);
  perms[perms_obj->len] = '\0';

  FILE *file = fopen(path, perms);
  if (file == NULL) {
    return GAB_VAL_NULL();
  }

  gab_value container =
      GAB_VAL_OBJ(gab_obj_container_create(gab_container_file_cb, file));

  return container;
}

gab_value gab_lib_read(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_TO_CONTAINER(argv[0])) {
    return GAB_VAL_NULL();
  }

  gab_obj_container *file_obj = GAB_VAL_TO_CONTAINER(argv[0]);
  if (file_obj->destructor != gab_container_file_cb) {
    return GAB_VAL_NULL();
  }

  FILE *file = file_obj->data;

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char buffer[fileSize];

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *result =
      gab_obj_string_create(eng, s_i8_create((i8 *)buffer + 0, bytesRead));

  return GAB_VAL_OBJ(result);
}

gab_value gab_lib_write(gab_engine *eng, gab_vm* vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_CONTAINER(argv[0]) ||
      !GAB_VAL_IS_STRING(argv[1])) {
    return GAB_VAL_NULL();
  }

  gab_obj_container *handle = GAB_VAL_TO_CONTAINER(argv[0]);
  if (handle->destructor != gab_container_file_cb || handle->data == NULL) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *data_obj = GAB_VAL_TO_STRING(argv[1]);
  char data[data_obj->len + 1];
  memcpy(data, data_obj->data, sizeof(i8) * data_obj->len);
  data[data_obj->len] = '\0';

  i32 result = fputs(data, handle->data);

  if (result > 0) {
    return GAB_VAL_BOOLEAN(true);
  } else {
    return GAB_VAL_NULL();
  }
}

gab_value gab_mod(gab_engine *gab, gab_vm* vm) {
  s_i8 keys[] = {
      s_i8_cstr("open"),
      s_i8_cstr("read"),
      s_i8_cstr("write"),
  };

  gab_value values[] = {
      GAB_BUILTIN(open),
      GAB_BUILTIN(read),
      GAB_BUILTIN(write),
  };

  return gab_bundle_record(gab, vm, LEN_CARRAY(values), keys, values);
}
