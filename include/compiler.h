#ifndef GAB_COMPILER_H
#define GAB_COMPILER_H

#include "gab.h"

gab_value gab_bccomp(struct gab_triple gab, gab_value name, s_char source,
                     uint8_t flags, uint8_t narguments,
                     gab_value arguments[narguments]);
#endif
