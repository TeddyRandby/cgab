#ifndef BLUF_LEXER_H
#define BLUF_LEXER_H

#include "core.h"

typedef enum gab_token {
#define TOKEN(name) TOKEN##_##name,
#include "include/token.h"
#undef TOKEN
} gab_token;

#define T gab_token
#include "include/vector.h"

typedef struct gab_eg gab_eg;
typedef struct gab_src gab_src;

struct gab_src {
  gab_src *next;

  a_int8_t *source;

  v_s_int8_t lines;

  v_s_int8_t line_comments;

  v_gab_token tokens;

  v_s_int8_t tokens_src;

  v_uint64_t tokens_line;
};

gab_src *gab_lex(gab_eg* gab, const char* src, size_t len);

gab_src *gab_srccpy(gab_eg * gab, gab_src *src);

void gab_srcfree(gab_src *self);

#endif
