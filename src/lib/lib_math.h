#ifndef GAB_LIB_MATH_H
#define GAB_LIB_MATH_H

#include "../gab/gab.h"

gab_value gab_lib_math_rand(u8 argc, gab_value *argv, gab_engine *eng,
                            char **err);

gab_value gab_lib_math_floor(u8 argc, gab_value *argv, gab_engine *eng,
                             char **err);

#endif
