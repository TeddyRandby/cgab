#include "include/gab.h"
#include <stdio.h>

#define T gab_value
#include "include/vector.h"

static inline void list_destroy(void *data) {
  v_gab_value *self = data;
  v_gab_value_destroy(self);
  DESTROY(self);
}

void list_visit(struct gab_gc *gc, gab_gcvisit_f v, void *data) {
  v_gab_value *self = data;
  for (uint64_t i = 0; i < self->len; i++) {
    gab_value val = v_gab_value_val_at(self, i);
    if (gab_valiso(val)) {
      v(gc, gab_valtoo(val));
    }
  }
}

static inline gab_value list_put(struct gab_eg *gab, struct gab_gc *gc,
                                 struct gab_vm *vm, gab_value self,
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

  if (vm)
    gab_gcdref(gab, gc, vm, v_gab_value_val_at(data, offset));

  v_gab_value_set(data, offset, value);

fin:
  if (vm)
    gab_gciref(gab, gc, vm, value);

  return value;
}

static inline gab_value list_at(gab_value self, uint64_t offset) {
  v_gab_value *data = gab_boxdata(self);

  if (offset >= data->len)
    return gab_nil;

  return v_gab_value_val_at(data, offset);
}

static inline gab_value list_create_empty(struct gab_eg *gab, struct gab_gc *gc,
                                          struct gab_vm *vm, uint64_t len) {
  v_gab_value *data = NEW(v_gab_value);
  v_gab_value_create(data, len);

  return gab_box(gab,
                 (struct gab_box_argt){
                     .type = gab_gciref(gab, gc, vm, gab_string(gab, "List")),
                     .destructor = list_destroy,
                     .visitor = list_visit,
                     .data = data,
                 });
}

static inline gab_value list_create(struct gab_eg *gab, struct gab_gc *gc,
                                    struct gab_vm *vm, uint64_t len,
                                    gab_value argv[len]) {
  gab_value self = list_create_empty(gab, gc, vm, len);

  for (uint64_t i = 0; i < len; i++)
    list_put(gab, gc, vm, self, i, argv[i]);

  return self;
}
