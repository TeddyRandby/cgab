#include "gab.h"
#include <stdio.h>

void file_cb(size_t len, char data[static len]) { fclose(*(FILE **)data); }

a_gab_value *gab_lib_open(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value path = gab_arg(1);
  gab_value perm = gab_arg(2);

  if (gab_valkind(path) != kGAB_STRING)
    return gab_ptypemismatch(gab, path, gab_type(gab, kGAB_STRING));

  if (gab_valkind(perm) != kGAB_STRING)
    return gab_ptypemismatch(gab, perm, gab_type(gab, kGAB_STRING));

  const char *cpath = gab_strdata(&path);
  const char *cperm = gab_strdata(&perm);

  FILE *file = fopen(cpath, cperm);

  if (file == nullptr) {
    gab_value r = gab_sigil(gab, "FILE_COULD_NOT_OPEN");
    gab_vmpush(gab_vm(gab), r);
    return nullptr;
  }

  gab_value result[2] = {
      gab_ok,
      gab_box(gab,
              (struct gab_box_argt){
                  .type = gab_string(gab, "File"),
                  .data = &file,
                  .size = sizeof(FILE *),
                  .destructor = file_cb,
                  .visitor = nullptr,
              }),
  };

  gab_nvmpush(gab_vm(gab), 2, result);

  return nullptr;
}

a_gab_value *gab_lib_read(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc != 1 || gab_valkind(argv[0]) != kGAB_BOX)
    return gab_panic(gab, "&:read expects a file handle");

  FILE *file = *(FILE **)gab_boxdata(argv[0]);

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char buffer[fileSize];

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

  if (bytesRead < fileSize) {
    gab_vmpush(gab_vm(gab), gab_string(gab, "FILE_COULD_NOT_READ"));
    return nullptr;
  }

  gab_vmpush(gab_vm(gab), gab_ok, gab_nstring(gab, bytesRead, buffer));

  return nullptr;
}

a_gab_value *gab_lib_write(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  if (argc != 2 || gab_valkind(argv[0]) != kGAB_BOX) {
    return gab_panic(gab, "&:write expects a file handle and data string");
  }

  FILE *file = gab_boxdata(argv[0]);

  const char *data = gab_strdata(argv + 1);

  int32_t result = fputs(data, file);

  if (result > 0) {
    gab_vmpush(gab_vm(gab), gab_ok);
    return nullptr;
  }

  gab_vmpush(gab_vm(gab), gab_sigil(gab, "FILE_COULD_NOT_WRITE"));
  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "io.open",
          gab_undefined,
          gab_snative(gab, "io.open", gab_lib_open),
      },
      {
          "read",
          gab_string(gab, "File"),
          gab_snative(gab, "read", gab_lib_read),
      },
      {
          "write",
          gab_string(gab, "File"),
          gab_snative(gab, "write", gab_lib_write),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return nullptr;
}
