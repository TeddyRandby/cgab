#include "core.h"
#include <stdio.h>

a_i8 *os_read_file(const char *path);

a_i8 *os_read_fd(FILE* fd);

a_i8 *os_read_fd_line(FILE* fd);

a_i8 *os_pwd();
