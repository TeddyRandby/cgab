#include "gab.h"
#include <errno.h>
#include <stdio.h>

#define GAB_IOSTREAM "gab.io.stream"

void file_cb(size_t len, char data[static len]) { fclose(*(FILE **)data); }

gab_value iostream(struct gab_triple gab, FILE *stream, bool owning) {
  return gab_box(gab, (struct gab_box_argt){
                          .type = gab_string(gab, GAB_IOSTREAM),
                          .data = &stream,
                          .size = sizeof(FILE *),
                          .destructor = owning ? file_cb : nullptr,
                          .visitor = nullptr,
                      });
}

a_gab_value *gab_lib_open(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value path = gab_arg(1);
  gab_value perm = gab_arg(2);

  if (gab_valkind(path) != kGAB_STRING)
    return gab_pktypemismatch(gab, path, kGAB_STRING);

  if (gab_valkind(perm) != kGAB_STRING)
    return gab_pktypemismatch(gab, perm, kGAB_STRING);

  const char *cpath = gab_strdata(&path);
  const char *cperm = gab_strdata(&perm);

  FILE *stream = fopen(cpath, cperm);

  if (stream == nullptr) {
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
    return nullptr;
  }

  gab_vmpush(gab_vm(gab), gab_ok, iostream(gab, stream, true));

  return nullptr;
}

a_gab_value *gab_lib_until(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  gab_value iostream = gab_arg(0);
  gab_value delim = gab_arg(1);

  if (gab_valkind(iostream) != kGAB_BOX)
    return gab_ptypemismatch(gab, iostream, gab_string(gab, GAB_IOSTREAM));

  if (delim == gab_nil)
    delim = gab_string(gab, "\n");

  v_char buffer;
  v_char_create(&buffer, 1024);

  FILE *stream = *(FILE **)gab_boxdata(argv[0]);

  for (;;) {
    char c = fgetc(stream);

    if (c == EOF)
      break;

    v_char_push(&buffer, c);

    if (c == '\n')
      break;
  }

  gab_vmpush(gab_vm(gab), gab_ok, gab_nstring(gab, buffer.len, buffer.data));

  return nullptr;
}

a_gab_value *gab_lib_scan(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value iostream = gab_arg(0);
  gab_value bytesToRead = gab_arg(1);

  if (gab_valkind(iostream) != kGAB_BOX)
    return gab_ptypemismatch(gab, iostream, gab_string(gab, GAB_IOSTREAM));

  if (gab_valkind(bytesToRead) != kGAB_NUMBER)
    return gab_pktypemismatch(gab, bytesToRead, kGAB_NUMBER);

  size_t bytes = gab_valton(bytesToRead);

  if (bytes == 0) {
    gab_vmpush(gab_vm(gab), gab_string(gab, ""));
    return nullptr;
  }

  char buffer[bytes];

  FILE *stream = *(FILE **)gab_boxdata(argv[0]);

  // Try to read bytes number of bytes
  int bytes_read = fread(buffer, 1, sizeof(buffer), stream);

  if (bytes_read < bytes)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
  else
    gab_vmpush(gab_vm(gab), gab_ok, gab_nstring(gab, bytes_read, buffer));

  return nullptr;
}

a_gab_value *gab_lib_read(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc != 1 || gab_valkind(argv[0]) != kGAB_BOX)
    return gab_fpanic(gab, "&:read expects a file handle");

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
  gab_value stream = gab_arg(0);

  if (gab_valkind(stream) != kGAB_BOX)
    return gab_ptypemismatch(gab, stream, gab_string(gab, GAB_IOSTREAM));

  FILE *fs = *(FILE **)gab_boxdata(stream);

  gab_value str = gab_arg(1);

  if (gab_valkind(str) != kGAB_STRING)
    return gab_pktypemismatch(gab, str, kGAB_STRING);

  const char *data = gab_strdata(&str);

  int32_t result = fputs(data, fs);

  if (result <= 0 || fflush(fs))
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
  else
    gab_vmpush(gab_vm(gab), gab_ok);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value iostream_t = gab_string(gab, GAB_IOSTREAM);

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_strtosig(iostream_t),
          gab_snative(gab, "iostream.open", gab_lib_open),
      },
      {
          "until",
          iostream_t,
          gab_snative(gab, "until", gab_lib_until),
      },
      {
          "scan",
          iostream_t,
          gab_snative(gab, "scan", gab_lib_scan),
      },
      {
          "read",
          iostream_t,
          gab_snative(gab, "read", gab_lib_read),
      },
      {
          "write",
          iostream_t,
          gab_snative(gab, "write", gab_lib_write),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  gab_value results[] = {
      gab_strtosig(iostream_t),
      iostream(gab, stdin, false),
      iostream(gab, stdout, false),
      iostream(gab, stderr, false),
  };

  return a_gab_value_create(results, sizeof(results) / sizeof(results[0]));
}