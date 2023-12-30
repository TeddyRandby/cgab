#ifndef BLUF_LEXER_H
#define BLUF_LEXER_H

#include "gab.h"

typedef enum gab_token {
#define TOKEN(name) TOKEN##_##name,
#include "token.h"
#undef TOKEN
} gab_token;

#define T gab_token
#include "vector.h"

struct gab_eg;
struct gab_src;

struct gab_src {
  gab_value name;

  a_char *source;

  v_s_char lines;

  v_gab_token tokens;

  v_s_char token_srcs;

  v_uint64_t token_lines;

  v_uint8_t bytecode;

  v_uint64_t bytecode_toks;

  v_gab_value constants;
};

struct gab_src *gab_src(struct gab_eg *gab, gab_value name, const char *src, size_t len);

struct gab_src *gab_srccpy(struct gab_triple gab, struct gab_src *src);

void gab_srcdestroy(struct gab_src *self);

#endif
