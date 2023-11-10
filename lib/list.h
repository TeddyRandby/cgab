#include "include/gab.h"
#include <stdio.h>

static inline void list_destroy(void *data) {
  v_gab_value *self = data;
  v_gab_value_destroy(self);
  DESTROY(self);
}

void list_visit(struct gab_triple gab, gab_gcvisit_f v, void *data) {
  v_gab_value *self = data;
  for (uint64_t i = 0; i < self->len; i++) {
    gab_value val = v_gab_value_val_at(self, i);
    if (gab_valiso(val)) {
      v(gab, gab_valtoo(val));
    }
  }
}

static inline gab_value list_put(struct gab_triple gab, gab_value self,
                                 uint64_t offset, gab_value value) {
  v_gab_value *data = gab_boxdata(self);

  if (offset >= data->len) {
    uint64_t nils = offset - data->len;

    while (nils-- > 0) {
      v_gab_value_push(data, gab_nil);
    }

    v_gab_value_push(data, value);

    goto fin;
  }

  v_gab_value_set(data, offset, value);

fin:
  return value;
}

static inline gab_value list_at(gab_value self, uint64_t offset) {
  v_gab_value *data = gab_boxdata(self);

  if (offset >= data->len)
    return gab_nil;

  return v_gab_value_val_at(data, offset);
}

static inline gab_value list_create_empty(struct gab_triple gab, uint64_t len) {
  v_gab_value *data = NEW(v_gab_value);
  v_gab_value_create(data, len);

  return gab_box(gab, (struct gab_box_argt){
                          .type = gab_string(gab, "List"),
                          .destructor = list_destroy,
                          .visitor = list_visit,
                          .data = data,
                      });
}

static inline gab_value list_create(struct gab_triple gab, uint64_t len,
                                    gab_value argv[len]) {
  gab_value self = list_create_empty(gab, len);

  for (uint64_t i = 0; i < len; i++)
    list_put(gab, self, i, argv[i]);

  return self;
}
