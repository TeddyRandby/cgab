#include "include/gab.h"
#include "include/value.h"
#include <stdio.h>

#define T gab_value
#include "include/vector.h"

void list_destroy(void *data) {
  v_gab_value *self = data;
  v_gab_value_destroy(self);
  DESTROY(self);
}

void list_visit(gab_gc *gc, gab_gcvisit_f v, void *data) {
  v_gab_value *self = data;
  for (u64 i = 0; i < self->len; i++) {
    gab_value val = v_gab_value_val_at(self, i);
    if (gab_valiso(val)) {
      v(gc, gab_valtoo(val));
    }
  }
}

gab_value list_put(gab_eg *gab, gab_vm *vm, gab_value self, u64 offset,
                   gab_value value) {
  v_gab_value *data = gab_boxdata(self);

  if (offset >= data->len) {
    u64 nils = offset - data->len;

    while (nils-- > 0) {
      v_gab_value_push(data, gab_nil);
    }

    v_gab_value_push(data, value);

    return value;
  }

  v_gab_value_set(data, offset, value);

  if (vm)
    gab_gciref(gab_vmgc(vm), vm, value);

  return value;
}

gab_value list_at(gab_value self, u64 offset) {
  v_gab_value *data = gab_boxdata(self);

  if (offset >= data->len)
    return gab_nil;

  return v_gab_value_val_at(data, offset);
}

gab_value list_create_empty(gab_eg *gab, gab_vm *vm, u64 len) {
  v_gab_value *data = NEW(v_gab_value);
  v_gab_value_create(data, len);

  return gab_box(gab, vm,
                 (struct gab_box_argt){
                     .type = gab_string(gab, "List"),
                     .destructor = list_destroy,
                     .visitor = list_visit,
                     .data = data,
                 });
}

gab_value list_create(gab_eg *gab, gab_vm *vm, u64 len, gab_value argv[len]) {
  gab_value self = list_create_empty(gab, vm, len);

  for (u64 i = 0; i < len; i++)
    list_put(gab, vm, self, i, argv[i]);

  return self;
}
