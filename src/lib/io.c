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

gab_value gab_lib_open(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc != 3 || !GAB_VAL_IS_STRING(argv[1]) || !GAB_VAL_IS_STRING(argv[2])) {
    return gab_result_err(gab, vm, GAB_STRING("Invalid call to gab_lib_open"));
  }

  gab_obj_string *path_obj = GAB_VAL_TO_STRING(argv[1]);
  char path[path_obj->len + 1];
  memcpy(path, path_obj->data, sizeof(i8) * path_obj->len);
  path[path_obj->len] = '\0';

  gab_obj_string *perms_obj = GAB_VAL_TO_STRING(argv[2]);
  char perms[perms_obj->len + 1];
  memcpy(perms, perms_obj->data, sizeof(i8) * perms_obj->len);
  perms[perms_obj->len] = '\0';

  FILE *file = fopen(path, perms);
  if (file == NULL) {
    return gab_result_err(gab, vm, GAB_STRING("Failed to open file"));
  }

  gab_value container =
      GAB_VAL_OBJ(gab_obj_container_create(gab_container_file_cb, file));

  return gab_result_ok(gab, vm, container);
}

gab_value gab_lib_read(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_TO_CONTAINER(argv[0])) {
    return gab_result_err(gab, vm, GAB_STRING("Invalid call to gab_lib_read"));
  }

  gab_obj_container *file_obj = GAB_VAL_TO_CONTAINER(argv[0]);
  if (file_obj->destructor != gab_container_file_cb) {
    return gab_result_err(gab, vm,
                          GAB_STRING("Invalid argument to gab_lib_read"));
  }

  FILE *file = file_obj->data;

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char buffer[fileSize];

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    return gab_result_err(gab, vm, GAB_STRING("Couldn't read all bytes"));
  }

  gab_obj_string *result =
      gab_obj_string_create(gab, s_i8_create((i8 *)buffer + 0, bytesRead));

  return gab_result_ok(gab, vm, GAB_VAL_OBJ(result));
}

gab_value gab_lib_write(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_CONTAINER(argv[0]) ||
      !GAB_VAL_IS_STRING(argv[1])) {
    return gab_result_err(gab, vm, GAB_STRING("Invalid call to gab_lib_write"));
  }

  gab_obj_container *handle = GAB_VAL_TO_CONTAINER(argv[0]);
  if (handle->destructor != gab_container_file_cb || handle->data == NULL) {
    return gab_result_err(gab, vm, GAB_STRING("Invalid call to gab_lib_write"));
  }

  gab_obj_string *data_obj = GAB_VAL_TO_STRING(argv[1]);
  char data[data_obj->len + 1];
  memcpy(data, data_obj->data, sizeof(i8) * data_obj->len);
  data[data_obj->len] = '\0';

  i32 result = fputs(data, handle->data);

  if (result > 0) {
    return gab_result_ok(gab, vm, GAB_VAL_NULL());
  } else {
    return gab_result_err(gab, vm, GAB_STRING("Failed to write file"));
  }
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value io = GAB_SYMBOL("io");

  s_i8 keys[] = {
      s_i8_cstr("open"),
      s_i8_cstr("read"),
      s_i8_cstr("write"),
  };

  gab_value container_type = gab_get_type(gab, TYPE_CONTAINER);

  gab_value receiver_types[] = {
      io,
      container_type,
      container_type,
  };

  gab_value values[] = {
      GAB_BUILTIN(open),
      GAB_BUILTIN(read),
      GAB_BUILTIN(write),
  };

  for (u8 i = 0; i < 3; i++) {
    gab_specialize(gab, keys[i], receiver_types[i], values[i]);
  }

  return io;
}
