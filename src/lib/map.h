#include "include/gab.h"

#define K gab_value
#define V gab_value
#define DEF_V GAB_VAL_UNDEFINED()
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#include "include/dict.h"

void map_destroy(void *data) {
  d_gab_value *self = data;
  d_gab_value_destroy(self);
  DESTROY(self);
}

void map_visit(gab_gc *gc, gab_gc_visitor v, void *data) {
  d_gab_value *map = data;
  for (u64 i = 0; i < map->cap; i++) {
    if (d_gab_value_iexists(map, i)) {
      gab_value key = d_gab_value_ikey(map, i);
      gab_value val = d_gab_value_ival(map, i);
      if (GAB_VAL_IS_OBJ(key))
        v(gc, GAB_VAL_TO_OBJ(key));
      if (GAB_VAL_IS_OBJ(val))
        v(gc, GAB_VAL_TO_OBJ(val));
    }
  }
}

gab_value map_at(gab_obj_container *self, gab_value key) {
  return d_gab_value_read(self->data, key);
}

boolean map_has(gab_obj_container *self, gab_value key) {
  return d_gab_value_exists(self->data, key);
}

gab_value map_put(gab_engine *gab, gab_vm *vm, gab_obj_container *self,
                  gab_value key, gab_value value) {
  if (vm) {
    if (d_gab_value_exists(self->data, key)) {
      gab_val_iref(vm, d_gab_value_read(self->data, key));
    } else {
      gab_val_iref(vm, key);
    }

    gab_val_iref(vm, value);
  }

  d_gab_value_insert(self->data, key, value);

  return value;
}

gab_obj_container *map_create(gab_engine *gab, gab_vm *vm, u64 len, u64 stride,
                              gab_value keys[len], gab_value values[len]) {
  d_gab_value *data = NEW(d_gab_value);
  d_gab_value_create(data, 8);

  gab_obj_container *self = gab_obj_container_create(
      gab, vm, GAB_STRING("Map"), map_destroy, map_visit, data);

  for (u64 i = 0; i < len; i++) {
    d_gab_value_insert(data, keys[i * stride], values[i * stride]);
  }

  if (vm) {
    gab_val_iref_many(vm, len, keys);
    gab_val_iref_many(vm, len, values);
  }

  return self;
}
