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

  v_gab_value constants;
  v_uint8_t bytecode;
  v_uint64_t bytecode_toks;

  size_t len;
  /**
   * Each OS thread needs its own copy of the bytecode and constants.
   * Both of these arrays are modified at runtime by the VM (for specializing
   * and inline cacheing)
   */
  struct src_bytecode {
    uint8_t *bytecode;
    gab_value *constants;
  } thread_bytecode[];
};

struct gab_src *gab_src(struct gab_triple gab, gab_value name, const char *src,
                        size_t len);

struct gab_src *gab_srccpy(struct gab_triple gab, struct gab_src *src);

size_t gab_srcappend(struct gab_src *self, size_t len, uint8_t bc[static len],
                     uint64_t toks[static len]);

static inline void gab_srccomplete(struct gab_triple gab,
                                   struct gab_src *self) {
  for (int i = 0; i < self->len; i++) {
    uint8_t *bc = malloc(self->bytecode.len);
    memcpy(bc, self->bytecode.data, self->bytecode.len);

    gab_value *ks = malloc(self->constants.len * sizeof(gab_value));
    memcpy(ks, self->constants.data, self->constants.len * sizeof(gab_value));

    self->thread_bytecode[i] = (struct src_bytecode){bc, ks};
  }
}

void gab_srcdestroy(struct gab_src *self);

#endif
