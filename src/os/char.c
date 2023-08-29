#include "include/char.h"

bool is_whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\f'; }

bool is_alpha_lower(uint8_t c) { return c >= 'a' && c <= 'z'; }

bool is_alpha_upper(uint8_t c) { return c >= 'A' && c <= 'Z'; }

bool is_alpha(uint8_t c) {
  return is_alpha_lower(c) || is_alpha_upper(c) || c == '_';
}

bool is_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_comment(uint8_t c) { return c == '#'; }
