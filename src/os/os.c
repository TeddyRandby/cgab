#include "include/os.h"
#include "include/core.h"
#include "include/types.h"
#include "stdio.h"

#if OS_UNIX
#include <unistd.h>
#endif

a_i8 *os_read_fd(FILE *fd) {
  v_i8 buffer;
  v_i8_create(&buffer, 1024);

  while (1) {
    char c = fgetc(fd);
    if (c == EOF) {
      break;
    }
    v_i8_push(&buffer, c);
  }

  v_i8_push(&buffer, '\0');

  a_i8 *data = a_i8_create(buffer.data, buffer.len);

  v_i8_destroy(&buffer);

  return data;
}

a_i8 *os_read_file(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  a_i8 *data = os_read_fd(file);

  fclose(file);
  return data;
}

a_i8 *os_read_fd_line(FILE *fd) {
  v_i8 buffer = {};

  for (;;) {
    char c = fgetc(fd);

    v_i8_push(&buffer, c);

    if (c == '\n' || c == EOF)
      break;
  }

  v_i8_push(&buffer, '\0');

  a_i8 *data = a_i8_create(buffer.data, buffer.len);

  v_i8_destroy(&buffer);

  return data;
}

#define BUFFER_MAX 1024
a_i8 *os_pwd() {
#if OS_UNIX
  a_i8 *result = a_i8_empty(BUFFER_MAX);
  getcwd((char *)result->data, BUFFER_MAX);

  return result;
#endif
}
