#include "include/gab.h"
#include <stdio.h>

void gab_container_file_cb(gab_engine *gab, gab_vm *vm, void *data) {
  if (fclose(data)) {
    gab_panic(gab, vm, "Failed to cleanup file container");
  }
}

gab_value gab_lib_open(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc != 3 || !GAB_VAL_IS_STRING(argv[1]) || !GAB_VAL_IS_STRING(argv[2])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_open");
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
    return GAB_SEND("err", GAB_STRING("Unable to open file"), 0, NULL);
  }

  gab_value container = GAB_CONTAINER(gab_container_file_cb, file);

  gab_dref(gab, vm, container);

  return GAB_SEND("ok", container, 0, NULL);
}

gab_value gab_lib_read(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_TO_CONTAINER(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_read");
  }

  gab_obj_container *file_obj = GAB_VAL_TO_CONTAINER(argv[0]);
  if (file_obj->destructor != gab_container_file_cb) {
    return GAB_SEND("err", GAB_STRING("Invalid argument to gab_lib_read"), 0,
                    NULL);
  }

  FILE *file = file_obj->data;

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char buffer[fileSize];

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    return GAB_SEND("err", GAB_STRING("Couldn't read all bytes"), 0, NULL);
  }

  gab_obj_string *result =
      gab_obj_string_create(gab, s_i8_create((i8 *)buffer + 0, bytesRead));

  return GAB_SEND("ok", GAB_VAL_OBJ(result), 0, NULL);
}

gab_value gab_lib_write(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_CONTAINER(argv[0]) ||
      !GAB_VAL_IS_STRING(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_write");
  }

  gab_obj_container *handle = GAB_VAL_TO_CONTAINER(argv[0]);
  if (handle->destructor != gab_container_file_cb || handle->data == NULL) {
    return GAB_SEND("err", GAB_STRING("Invalid call to gab_lib_write"), 0,
                    NULL);
  }

  gab_obj_string *data_obj = GAB_VAL_TO_STRING(argv[1]);
  char data[data_obj->len + 1];
  memcpy(data, data_obj->data, sizeof(i8) * data_obj->len);
  data[data_obj->len] = '\0';

  i32 result = fputs(data, handle->data);

  if (result > 0) {
    return GAB_SEND("ok", GAB_VAL_NIL(), 0, NULL);
  } else {
    return GAB_SEND("err", GAB_STRING("Failed to write file"), 0, NULL);
  }
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value io = GAB_SYMBOL("io");
  gab_dref(gab, vm, io);

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
    gab_dref(gab, vm, values[i]);
  }

  return io;
}
