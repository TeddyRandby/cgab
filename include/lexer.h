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

typedef struct gab_eg gab_eg;
typedef struct gab_src gab_src;
typedef struct gab_lex gab_lex;

struct gab_src {
  gab_src *next;

  v_s_i8 source_lines;

  a_i8 *source;
};

struct gab_lex {
  i8 *cursor;
  i8 *row_start;
  u64 row;
  u64 col;

  u8 nested_curly;
  u8 status;

  gab_src* source;

  s_i8 previous_comment;
  s_i8 previous_row_src;
  s_i8 previous_token_src;
  u64 previous_row;
  gab_token previous_token;

  s_i8 current_row_src;
  s_i8 current_token_src;
  u64 current_row;
  gab_token current_token;

  u64 skip_lines;
};

gab_src* gab_srccreate(gab_eg* gab, s_i8 source);

gab_src* gab_srccpy(gab_eg* gab, gab_src* self);

void gab_srcdestroy(gab_src* self);

void gab_lexcreate(gab_lex *self, gab_src* src);

gab_token gab_lexnxt(gab_lex *self);

void gab_lexendl(gab_lex *self);

#endif
