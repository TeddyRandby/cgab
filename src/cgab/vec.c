#include "gab.h"

gab_value gab_vector(struct gab_triple gab, size_t stride, size_t len,
                     gab_value data[static len]);

gab_value gab_vecat(struct gab_triple gab, gab_value vec, gab_value key);

gab_value gab_vecput(struct gab_triple gab, gab_value vec, size_t key,
                     gab_value val);

gab_value gab_vecpush(struct gab_triple gab, gab_value vec, gab_value val);

gab_value gab_vecpop(struct gab_triple gab, gab_value vec, gab_value *valout);

void gab_mvecput(struct gab_triple gab, gab_value vec, size_t key,
                 gab_value val);

void gab_mvecpush(struct gab_triple gab, gab_value vec, gab_value val);

void gab_mvecpop(struct gab_triple gab, gab_value vec, gab_value *valout);
