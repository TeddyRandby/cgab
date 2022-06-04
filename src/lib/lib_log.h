#ifndef GAB_LIB_LOG_H
#define GAB_LIB_LOG_H

#include "../gab/gab.h"

gab_value gab_lib_log_error(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err);

gab_value gab_lib_log_warn(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err);

gab_value gab_lib_log_info(u8 argc, gab_value *argv, gab_engine *eng,
                           char **err);

gab_value gab_lib_log_print(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err);

#endif
