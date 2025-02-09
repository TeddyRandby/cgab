#include "gab.h"
#include "os.h"

a_char *gab_fosread(FILE *fd) {
  v_char buffer = {0};

  while (1) {
    int c = fgetc(fd);

    if (c == EOF)
      break;

    v_char_push(&buffer, c);
  }

  v_char_push(&buffer, '\0');

  a_char *data = a_char_create(buffer.data, buffer.len);

  v_char_destroy(&buffer);

  return data;
}

a_char *gab_osread(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == nullptr)
    return nullptr;

  a_char *data = gab_fosread(file);

  fclose(file);
  return data;
}

a_char *gab_fosreadl(FILE *fd) {
  v_char buffer;
  v_char_create(&buffer, 1024);

  for (;;) {
    int c = fgetc(fd);

    v_char_push(&buffer, c);

    if (c == '\n' || c == EOF)
      break;
  }

  v_char_push(&buffer, '\0');

  a_char *data = a_char_create(buffer.data, buffer.len);

  v_char_destroy(&buffer);

  return data;
}
