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

struct gab_eg;
struct gab_src;

struct gab_src {
  struct gab_src *next;

  a_char *source;

  v_s_char lines;

  v_s_char line_comments;

  v_gab_token tokens;

  v_s_char tokens_src;

  v_uint64_t tokens_line;
};

struct gab_src *gab_lex(struct gab_eg* gab, const char* src, size_t len);

struct gab_src *gab_srccpy(struct gab_eg * gab, struct gab_src *src);

void gab_srcdestroy(struct gab_src *self);

#endif
