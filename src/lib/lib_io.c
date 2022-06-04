#include "lib_io.h"

gab_value gab_lib_io_read(u8 argc, gab_value *argv, gab_engine *eng,
                          char **err) {
  if (argc != 1) {
    *err = "Expected one argument";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    *err = "Expected argument to be a string";
    return GAB_VAL_NULL();
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);
  char *cpath = (char *)path->data;

  FILE *file = fopen(cpath, "rb");
  if (file == NULL) {
    *err = "Could not read requested file";
    return GAB_VAL_NULL();
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = CREATE_ARRAY(char, fileSize + 1);
  if (buffer == NULL) {
    fclose(file);
    DESTROY_ARRAY(buffer);
    exit(1);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    *err = "Could not read requested file";
    fclose(file);
    DESTROY_ARRAY(buffer);
    return GAB_VAL_NULL();
  }

  buffer[bytesRead] = '\0';

  fclose(file);

  gab_obj_string *result =
      gab_obj_string_create(eng, s_u8_ref_create_cstr(buffer));

  DESTROY_ARRAY(buffer);
  return GAB_VAL_OBJ(result);
}

gab_value gab_lib_io_write(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err) {
  if (argc != 2) {
    *err = "Expected two arguments";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[0])) {
    *err = "Expected path argument to be a string";
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[1])) {
    *err = "Expected data argument to be a string";
    return GAB_VAL_NULL();
  }

  gab_obj_string *path = GAB_VAL_TO_STRING(argv[0]);
  char *cpath = (char *)path->data;
  gab_obj_string *data = GAB_VAL_TO_STRING(argv[1]);
  char *cdata = (char *)data->data;

  FILE *file = fopen(cpath, "w");
  if (file == NULL) {
    *err = "Could not open requested file";
    return GAB_VAL_NULL();
  }

  i32 result = fputs(cdata, file);
  fclose(file);

  if (result > 0) {
    return GAB_VAL_BOOLEAN(true);
  } else {
    *err = "Could not write requested file";
    return GAB_VAL_NULL();
  }
}
