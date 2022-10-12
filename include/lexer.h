#ifndef BLUF_LEXER_H
#define BLUF_LEXER_H

#include "core.h"

typedef enum gab_token {
#define TOKEN(name) TOKEN##_##name,
#include "token.h"
#undef TOKEN
} gab_token;

typedef struct gab_lexer gab_lexer;
struct gab_lexer {

  i8 *cursor;
  i8 *row_start;
  u64 row;
  u64 col;

  u8 nested_curly;

  s_i8 source;
  v_s_i8 *source_lines;

  s_i8 previous_row_src;
  s_i8 previous_token_src;
  u64 previous_row;

  s_i8 current_row_src;
  s_i8 current_token_src;
  u64 current_row;

  const char *error_msg;

  u64 skip_lines;
};

void gab_lexer_create(gab_lexer *self, s_i8 src);
gab_token gab_lexer_next(gab_lexer *self);
void gab_lexer_finish_line(gab_lexer *self);

#endif
