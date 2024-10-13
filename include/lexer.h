#ifndef BLUF_LEXER_H
#define BLUF_LEXER_H

#include "gab.h"

struct gab_eg;
struct gab_src;

struct gab_src *gab_src(struct gab_triple gab, gab_value name, const char *src,
                        size_t len);

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
