#ifndef GAB_COMPILER_H
#define GAB_COMPILER_H

#include "include/core.h"
#include "lexer.h"
#include "module.h"

typedef enum {
  fCAPTURED = 1 << 0,
  fMUTABLE = 1 << 1,
  fLOCAL = 1 << 2,
} variable_flag;

gab_value gab_bccompsend(gab_eg *gab, gab_value msg,
                              gab_value receiver, uint8_t flags, uint8_t narguments,
                              gab_value arguments[narguments]);

gab_value gab_bccomp(gab_eg *gab, gab_value name, s_int8_t source, uint8_t flags,
                         uint8_t narguments, gab_value arguments[narguments]);
#endif
