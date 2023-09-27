#include "core.h"
#include <stdio.h>

a_char *os_read_file(const char *path);

a_char *os_read_fd(FILE* fd);

a_char *os_read_fd_line(FILE* fd);

a_char *os_pwd();
