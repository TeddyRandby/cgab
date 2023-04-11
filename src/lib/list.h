#include "include/gab.h"

#define T gab_value
#include "include/vector.h"

void list_destroy(void *data) {
  v_gab_value *self = data;
  v_gab_value_destroy(self);
  DESTROY(self);
}

void list_visit(gab_gc *gc, gab_gc_visitor v, void *data) {
  v_gab_value *self = data;
  for (u64 i = 0; i < self->len; i++) {
    gab_value val = v_gab_value_val_at(self, i);

    if (GAB_VAL_IS_OBJ(val))
      v(gc, GAB_VAL_TO_OBJ(val));
  }
}

gab_value list_put(gab_engine *gab, gab_vm *vm, gab_obj_container *self,
                          u64 offset, gab_value value) {
  v_gab_value *data = self->data;

  if (offset >= data->len) {
    u64 nils = offset - data->len;

    while (nils-- > 0) {
      v_gab_value_push(data, GAB_VAL_NIL());
    }

    v_gab_value_push(data, value);

    return value;
  }

  v_gab_value_set(data, offset, value);

  if (vm)
    gab_val_iref(vm, value);

  return value;
}

gab_value list_at(gab_obj_container *self, u64 offset) {
  v_gab_value *data = self->data;

  if (offset >= data->len)
    return GAB_VAL_NIL();

  return v_gab_value_val_at(data, offset);
}

gab_obj_container *list_create_empty(gab_engine *gab, gab_vm *vm, u64 len) {
  v_gab_value *data = NEW(v_gab_value);
  v_gab_value_create(data, len);

  gab_obj_container *self = gab_obj_container_create(
      gab, vm, GAB_STRING("List"), list_destroy, list_visit, data);

  return self;
}

gab_obj_container *list_create(gab_engine *gab, gab_vm *vm, u64 len,
                                      gab_value argv[len]) {
    gab_obj_container *self = list_create_empty(gab, vm, len);

    for (u64 i = 0; i < len; i++)
      list_put(gab, vm, self, i, argv[i]);

    return self;
}
