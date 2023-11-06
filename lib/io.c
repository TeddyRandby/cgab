#include "include/core.h"
#include "include/gab.h"
#include <stdio.h>

void file_cb(void *data) { fclose(data); }

void gab_lib_open(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 3 || gab_valknd(argv[1]) != kGAB_STRING ||
      gab_valknd(argv[2]) != kGAB_STRING) {
    gab_panic(gab, "&:open expects a path and permissions string");
    return;
  }

  s_char path = gab_valintocs(gab, argv[1]);
  s_char perm = gab_valintocs(gab, argv[2]);

  char cpath[path.len + 1];
  memcpy(cpath, path.data, path.len);
  cpath[path.len] = '\0';

  char cperm[perm.len + 1];
  memcpy(cperm, perm.data, perm.len);
  cperm[perm.len] = '\0';

  FILE *file = fopen(cpath, cperm);

  if (file == NULL) {
    gab_value r = gab_string(gab, "FILE_COULD_NOT_OPEN");
    gab_vmpush(gab.vm, r);
    return;
  }

  gab_value result[2] = {
      gab_string(gab, "ok"),
      gab_box(gab,
              (struct gab_box_argt){
                  .type = gab_string(gab, "File"),
                  .data = file,
                  .destructor = file_cb,
                  .visitor = NULL,
              }),
  };

  gab_nvmpush(gab.vm, 2, result);
}

void gab_lib_read(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1 || gab_valknd(argv[0]) != kGAB_BOX) {
    gab_panic(gab, "&:read expects a file handle");
    return;
  }

  FILE *file = gab_boxdata(argv[0]);

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char buffer[fileSize];

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

  if (bytesRead < fileSize) {
    gab_vmpush(gab.vm, gab_string(gab, "FILE_COULD_NOT_READ"));
    return;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_nstring(gab, bytesRead, buffer),
  };

  gab_nvmpush(gab.vm, 2, res);
}

void gab_lib_write(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[0]) != kGAB_BOX) {
    gab_panic(gab, "&:write expects a file handle and data string");
    return;
  }

  FILE *file = gab_boxdata(argv[0]);

  s_char data = gab_valintocs(gab, argv[1]);

  char cdata[data.len + 1];
  memcpy(cdata, data.data, data.len);
  cdata[data.len] = '\0';

  int32_t result = fputs(cdata, file);

  if (result > 0) {
    gab_vmpush(gab.vm, gab_string(gab, "ok"));
    return;
  }

  gab_vmpush(gab.vm, gab_string(gab, "FILE_COULD_NOT_WRITE"));
  return;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  const char *names[] = {
      "io.open",
      "read",
      "write",
  };

  gab_value type = gab_string(gab, "File");

  gab_value receivers[] = {
      gab_nil,
      type,
      type,
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "io.open", gab_lib_open),
      gab_sbuiltin(gab, "read", gab_lib_read),
      gab_sbuiltin(gab, "write", gab_lib_write),
  };

  for (uint8_t i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = specs[i],
                  });
  }

  return a_gab_value_one(type);
}
