#ifndef GAB_LIB_H
#define GAB_LIB_H
#include "../../gab/gab.h"

gab_value gab_lib_error(gab_engine *, gab_value *, u8);

gab_value gab_lib_warn(gab_engine *, gab_value *, u8);

gab_value gab_lib_info(gab_engine *, gab_value *, u8);

gab_value gab_lib_require(gab_engine *, gab_value *, u8);

gab_value gab_lib_keys(gab_engine *, gab_value *, u8);

gab_value gab_lib_len(gab_engine *, gab_value *, u8);

gab_value gab_lib_push(gab_engine *, gab_value *, u8);

gab_value gab_lib_num(gab_engine *, gab_value *, u8);

gab_value gab_lib_slice(gab_engine *eng, gab_value *argv, u8 argc);

/* BUNDLE SOCKET */
gab_value gab_lib_sock(gab_engine *, gab_value *, u8);

gab_value gab_lib_bind(gab_engine *, gab_value *, u8);

gab_value gab_lib_listen(gab_engine *, gab_value *, u8);

gab_value gab_lib_connect(gab_engine *, gab_value *, u8);

gab_value gab_lib_accept(gab_engine *, gab_value *, u8);

gab_value gab_lib_send(gab_engine *, gab_value *, u8);

gab_value gab_lib_recv(gab_engine *, gab_value *, u8);

gab_value gab_lib_close(gab_engine *, gab_value *, u8);
/* END BUNDLE SOCKET */

/* BUNDLE REGEX */
gab_value gab_lib_exec(gab_engine *, gab_value *, u8);

/* END BUNDLE REGEX*/
#endif
