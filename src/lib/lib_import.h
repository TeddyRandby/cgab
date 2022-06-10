#ifndef GAB_LIB_IMPORT_H
#define GAB_LIB_IMPORT_H

#include "../gab/gab.h"

gab_value gab_lib_require(u8 argc, gab_value *argv, gab_engine *eng,
                          char **err);

#endif
