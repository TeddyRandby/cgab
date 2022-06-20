#ifndef GAB_LIB_H
#define GAB_LIB_H
#include "../gab/gab.h"

/* LOGGING */
gab_value gab_lib_log_error(gab_engine *, gab_value *, u8);

gab_value gab_lib_log_warn(gab_engine *, gab_value *, u8);

gab_value gab_lib_log_info(gab_engine *, gab_value *, u8);

/* REQUIRE */
gab_value gab_lib_require_require(gab_engine *, gab_value *, u8);

/* MATH */

/* OBJECT */
gab_value gab_lib_obj_keys(gab_engine *, gab_value *, u8);

#endif
