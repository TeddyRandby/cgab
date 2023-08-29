#include "core.h"
#include <stdio.h>

a_int8_t *os_read_file(const char *path);

a_int8_t *os_read_fd(FILE* fd);

a_int8_t *os_read_fd_line(FILE* fd);

a_int8_t *os_pwd();
