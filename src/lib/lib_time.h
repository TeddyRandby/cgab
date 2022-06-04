#ifndef GAB_LIB_TIME_H
#define GAB_LIB_TIME_H

#include "../gab/gab.h"

gab_value gab_lib_time_clock(u8 argc, gab_value *argv, gab_engine *eng,
                             char **err);

gab_value gab_lib_time_sleep(u8 argc, gab_value *argv, gab_engine *eng,
                             char **err);

#endif
