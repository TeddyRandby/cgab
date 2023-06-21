#include "include/char.h"

boolean is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\f'; }

boolean is_alpha_lower(u8 c) { return c >= 'a' && c <= 'z'; }

boolean is_alpha_upper(u8 c) { return c >= 'A' && c <= 'Z'; }

boolean is_alpha(u8 c) {
  return is_alpha_lower(c) || is_alpha_upper(c) || c == '_';
}

boolean is_digit(u8 c) { return c >= '0' && c <= '9'; }

boolean is_comment(u8 c) { return c == '#'; }
