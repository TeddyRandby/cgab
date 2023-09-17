#include "include/core.h"
#include "include/gab.h"
#include <stdio.h>

void file_cb(void *data) { fclose(data); }

void gab_lib_open(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 3 || gab_valknd(argv[1]) != kGAB_STRING ||
      gab_valknd(argv[2]) != kGAB_STRING) {
    gab_panic(gab, vm, "&:open expects a path and permissions string");
    return;
  }

  char *path = gab_valtocs(gab, argv[1]);
  char *perm = gab_valtocs(gab, argv[2]);

  FILE *file = fopen(path, perm);

  free(path);
  free(perm);

  if (file == NULL) {
    gab_value r = gab_string(gab, "FILE_COULD_NOT_OPEN");
    gab_vmpush(vm, r);
    return;
  }

  gab_value result[2] = {
      gab_string(gab, "ok"),
      gab_box(gab, vm,
              (struct gab_box_argt){
                  .type = gab_string(gab, "File"),
                  .data = file,
                  .destructor = file_cb,
                  .visitor = NULL,
              }),
  };

  gab_nvmpush(vm, 2, result);

  gab_gcdref(gab_vmgc(vm), vm, result[1]);
}

void gab_lib_read(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1 || gab_valknd(argv[0]) != kGAB_BOX) {
    gab_panic(gab, vm, "&:read expects a file handle");
    return;
  }

  FILE *file = gab_boxdata(argv[0]);

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char buffer[fileSize];

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

  if (bytesRead < fileSize) {
    gab_vmpush(vm, gab_string(gab, "FILE_COULD_NOT_READ"));
    return;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_nstring(gab, bytesRead, buffer),
  };

  gab_nvmpush(vm, 2, res);

  gab_gcdref(gab_vmgc(vm), vm, res[1]);
}

void gab_lib_write(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[0]) != kGAB_BOX) {
    gab_panic(gab, vm, "&:write expects a file handle and data string");
    return;
  }

  FILE *file = gab_boxdata(argv[0]);

  char *data = gab_valtocs(gab, argv[1]);

  int32_t result = fputs(data, file);

  free(data);

  if (result > 0) {
    gab_vmpush(vm, gab_string(gab, "ok"));
    return;
  }

  gab_vmpush(vm, gab_string(gab, "FILE_COULD_NOT_WRITE"));
  return;
}

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "open",
      "read",
      "write",
  };

  gab_value type = gab_string(gab, "File");

  gab_value receiver_types[] = {
      gab_nil,
      type,
      type,
  };

  gab_value values[] = {
      gab_builtin(gab, "open", gab_lib_open),
      gab_builtin(gab, "read", gab_lib_read),
      gab_builtin(gab, "write", gab_lib_write),
  };

  for (uint8_t i = 0; i < LEN_CARRAY(values); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = receiver_types[i],
                 .specialization = values[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(values), values);

  return a_gab_value_one(type);
}
