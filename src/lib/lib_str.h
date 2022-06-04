#ifndef GAB_LIB_STR_H
#define GAB_LIB_STR_H

#include "../gab/gab.h"

gab_value gab_lib_str_split(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err);

gab_value gab_lib_str_tonum(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err);

gab_value gab_lib_str_sub(u8 argc, gab_value *argv, gab_engine *eng,
                          char **err);
#endif
