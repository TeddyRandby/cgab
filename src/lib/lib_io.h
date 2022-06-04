#ifndef GAB_LIB_IO_H
#define GAB_LIB_IO_H

#include "../gab/gab.h"

gab_value gab_lib_io_read(u8 argc, gab_value *argv, gab_engine *eng,
                          char **err);

gab_value gab_lib_io_write(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err);

#endif
