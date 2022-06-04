#ifndef BLUF_LEXER_H
#define BLUF_LEXER_H

#include "../../common/common.h"
#include "token.h"

typedef struct gab_lexer gab_lexer;
struct gab_lexer {

  u8 *cursor;
  u8 *row_start;
  u64 row;
  u64 col;

  u8 nested_curly;

  s_u8_ref source;
  v_s_u8_ref *source_lines;

  s_u8_ref previous_row_src;
  u64 previous_row;
  s_u8_ref previous_token_src;

  s_u8_ref current_row_src;
  u64 current_row;
  s_u8_ref current_token_src;

  const char *error_msg;

  u64 skip_lines;
};

void gab_lexer_create(gab_lexer *self, s_u8_ref src);
gab_token gab_lexer_next(gab_lexer *self);
void gab_lexer_finish_line(gab_lexer *self);

#endif
