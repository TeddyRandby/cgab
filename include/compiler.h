#ifndef GAB_COMPILER_H
#define GAB_COMPILER_H

#include "include/core.h"
#include "lexer.h"
#include "module.h"

enum variable_flag {
  fVAR_CAPTURED = 1 << 0,
  fVAR_MUTABLE = 1 << 1,
  fVAR_LOCAL = 1 << 2,
};

gab_value gab_bccompsend(struct gab_triple gab, gab_value msg,
                         gab_value receiver, uint8_t flags, uint8_t narguments,
                         gab_value arguments[narguments]);

gab_value gab_bccomp(struct gab_triple gab, gab_value name, s_char source,
                     uint8_t flags, uint8_t narguments,
                     gab_value arguments[narguments]);
#endif
