#ifndef BLUF_LEXER_H
#define BLUF_LEXER_H

#include "core.h"

typedef enum gab_token {
#define TOKEN(name) TOKEN##_##name,
#include "token.h"
#undef TOKEN
} gab_token;

typedef enum gab_status {
#define STATUS(name, message) GAB_##name,
#include "include/status_code.h"
#undef STATUS
} gab_status;

typedef struct gab_engine gab_engine;
typedef struct gab_source gab_source;
typedef struct gab_lexer gab_lexer;

struct gab_source {
  gab_source *next;
  /*
     A vector of each line of source code.
  */
  v_s_i8 source_lines;

  /*
     A copy of the source code
  */
  a_i8 *source;
};

struct gab_lexer {

  i8 *cursor;
  i8 *row_start;
  u64 row;
  u64 col;

  u8 nested_curly;
  u8 status;

  gab_source* source;

  s_i8 previous_row_src;
  s_i8 previous_token_src;
  u64 previous_row;

  s_i8 current_row_src;
  s_i8 current_token_src;
  u64 current_row;

  u64 skip_lines;
};

gab_source* gab_source_create(gab_engine* gab, s_i8 source);

gab_source* gab_source_copy(gab_engine* gab, gab_source* self);

void gab_source_destroy(gab_source* self);

void gab_lexer_create(gab_lexer *self, gab_source* src);

gab_token gab_lexer_next(gab_lexer *self);

void gab_lexer_finish_line(gab_lexer *self);

#endif
