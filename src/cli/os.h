#ifndef gab_io_h
#define gab_io_h

#include "../common/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s_u8 *gab_os_read_file(const char *path);

s_u8 *gab_os_read_line();

#endif
