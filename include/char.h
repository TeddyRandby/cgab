#ifndef GAB_CHAR_H
#define GAB_CHAR_H

#include "core.h"

bool is_whitespace(uint8_t c);

bool is_alpha_lower(uint8_t c);

bool is_alpha_upper(uint8_t c);

bool is_alpha(uint8_t c);

bool is_digit(uint8_t c);

bool is_comment(uint8_t c);

bool can_start_identifier(uint8_t c);

bool can_continue_identifier(uint8_t c);

#endif
