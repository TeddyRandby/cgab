#include "os.h"
#include "core.h"
#include "types.h"
#include "stdio.h"

#if OS_UNIX
#include <unistd.h>
#endif

a_char *os_read_fd(FILE *fd) {
  v_char buffer = {0};

  while (1) {
    char c = fgetc(fd);

    if (c == EOF)
      break;

    v_char_push(&buffer, c);
  }

  v_char_push(&buffer, '\0');

  a_char *data = a_char_create(buffer.data, buffer.len);

  v_char_destroy(&buffer);

  return data;
}

a_char *os_read_file(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  a_char *data = os_read_fd(file);

  fclose(file);
  return data;
}

a_char *os_read_fd_line(FILE *fd) {
  v_int8_t buffer;
  v_int8_t_create(&buffer, 1024);

  for (;;) {
    char c = fgetc(fd);

    v_int8_t_push(&buffer, c);

    if (c == '\n' || c == EOF)
      break;
  }

  v_int8_t_push(&buffer, '\0');

  a_char *data = a_char_create(buffer.data, buffer.len);

  v_int8_t_destroy(&buffer);

  return data;
}

#define BUFFER_MAX 1024
a_char *os_pwd() {
#if OS_UNIX
  a_char *result = a_char_empty(BUFFER_MAX);
  getcwd((char *)result->data, BUFFER_MAX);

  return result;
#endif
}
