#include "os.h"

s_u8 *gab_os_read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead] = '\0';

  fclose(file);

  s_u8 *result = s_u8_create_cstr(buffer);
  free(buffer);
  return result;
}

s_u8 *gab_os_read_line() {
  char data[1024];

  if (fgets(data, sizeof(data), stdin)) {
    return s_u8_create_cstr(data);
  }

  return NULL;
}
