#include "include/engine.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/gc.h"
#include "include/module.h"
#include "include/object.h"
#include "include/types.h"
#include "include/vm.h"

#include <stdarg.h>
#include <stdio.h>

/**
 * Gab API stuff
 */
gab_engine *gab_create(u8 flags) {
  gab_engine *gab = NEW(gab_engine);

  d_gab_intern_create(&gab->interned, INTERN_INITIAL_CAP);

  gab_gc_create(&gab->gc);

  gab->flags = flags;

  gab->types[TYPE_NULL] = GAB_SYMBOL("null");
  gab->types[TYPE_NUMBER] = GAB_SYMBOL("number");
  gab->types[TYPE_BOOLEAN] = GAB_SYMBOL("boolean");
  gab->types[TYPE_STRING] = GAB_SYMBOL("string");
  gab->types[TYPE_FUNCTION] = GAB_SYMBOL("function");
  gab->types[TYPE_PROTOTYPE] = GAB_SYMBOL("prototype");
  gab->types[TYPE_BUILTIN] = GAB_SYMBOL("builtin");
  gab->types[TYPE_CLOSURE] = GAB_SYMBOL("closure");
  gab->types[TYPE_UPVALUE] = GAB_SYMBOL("upvalue");
  gab->types[TYPE_SHAPE] = GAB_SYMBOL("shape");
  gab->types[TYPE_SYMBOL] = GAB_SYMBOL("symbol");
  gab->types[TYPE_CONTAINER] = GAB_SYMBOL("container");

  return gab;
}

static inline void dref_all(gab_engine *gab) {
  for (u64 i = 0; i < gab->interned.cap; i++) {
    if (d_gab_intern_iexists(&gab->interned, i)) {
      gab_value v = d_gab_intern_ikey(&gab->interned, i);
      gab_dref(gab, NULL, v);
    }
  }
  for (u8 i = 0; i < GAB_NTYPES; i++) {
    gab_dref(gab, NULL, gab->types[i]);
  }
}

void gab_destroy(gab_engine *gab) {
  dref_all(gab);

  d_gab_intern_destroy(&gab->interned);

  gab_gc_collect(&gab->gc, NULL);
  gab_gc_destroy(&gab->gc);

  DESTROY(gab);
}

gab_module *gab_compile(gab_engine *gab, s_i8 name, s_i8 source, u8 narguments,
                        s_i8 arguments[narguments]) {
  return gab_bc_compile(gab, name, source, narguments, arguments);
}

gab_value gab_run(gab_engine *gab, gab_module *main, u8 argc,
                  gab_value argv[argc]) {
  gab_value result = gab_vm_run(gab, main, argc, argv);
  return result;
};

void gab_dref(gab_engine *gab, gab_vm *vm, gab_value value) {
  gab_gc_dref(&gab->gc, vm, value);
};

void gab_iref(gab_engine *gab, gab_vm *vm, gab_value value) {
  gab_gc_dref(&gab->gc, vm, value);
};

gab_value gab_bundle_record(gab_engine *gab, gab_vm *vm, u64 size,
                            s_i8 keys[size], gab_value values[size]) {
  gab_value value_keys[size];

  for (u64 i = 0; i < size; i++) {
    value_keys[i] = GAB_VAL_OBJ(gab_obj_string_create(gab, keys[i]));
  }

  gab_obj_shape *bundle_shape =
      gab_obj_shape_create(gab, vm, size, 1, value_keys);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(bundle_shape, size, 1, values));

  return bundle;
}

gab_value gab_bundle_array(gab_engine *gab, gab_vm *vm, u64 size,
                           gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create_array(gab, vm, size);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(bundle_shape, size, 1, values));

  return bundle;
}

gab_value gab_bundle_function(gab_engine *gab, gab_vm *vm, s_i8 name, u64 size,
                              gab_value *receivers,
                              gab_value *specializations) {
  gab_obj_function *f = gab_obj_function_create(gab, name);

  for (u64 i = 0; i < size; i++) {
    gab_obj_function_set(f, receivers[i], specializations[i]);
  }

  return GAB_VAL_OBJ(f);
}

/**
 * Gab internal stuff
 */

u16 gab_engine_intern(gab_engine *self, gab_value value) {
  d_gab_intern_insert(&self->interned, value, 0);

  u64 val = d_gab_intern_index_of(&self->interned, value);

  return val;
}

gab_obj_function *gab_engine_find_function(gab_engine *self, s_i8 name,
                                           u64 hash) {
  if (self->interned.len == 0)
    return NULL;

  u64 index = hash & (self->interned.cap - 1);

  for (;;) {
    gab_value key = d_gab_intern_ikey(&self->interned, index);
    d_status status = d_gab_intern_istatus(&self->interned, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_FUNCTION(key)) {
      gab_obj_function *f_key = GAB_VAL_TO_FUNCTION(key);
      if (f_key->hash == hash && s_i8_match(name, f_key->name)) {
        return f_key;
      }
    }

    index = (index + 1) & (self->interned.cap - 1);
  }
}

gab_obj_string *gab_engine_find_string(gab_engine *self, s_i8 str, u64 hash) {
  if (self->interned.len == 0)
    return NULL;

  u64 index = hash & (self->interned.cap - 1);

  for (;;) {
    gab_value key = d_gab_intern_ikey(&self->interned, index);
    d_status status = d_gab_intern_istatus(&self->interned, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_STRING(key)) {
      gab_obj_string *str_key = GAB_VAL_TO_STRING(key);
      if (str_key->hash == hash &&
          s_i8_match(str, gab_obj_string_ref(str_key))) {
        return str_key;
      }
    }

    index = (index + 1) & (self->interned.cap - 1);
  }
}

static inline boolean shape_matches_keys(gab_obj_shape *self,
                                         gab_value values[], u64 size,
                                         u64 stride) {

  if (self->properties.len != size) {
    return false;
  }

  for (u64 i = 0; i < size; i++) {
    gab_value key = values[i * stride];
    if (!d_u64_exists(&self->properties, key)) {
      return false;
    }
  }

  return true;
}

gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]) {
  if (self->interned.len == 0)
    return NULL;

  u64 index = hash & (self->interned.cap - 1);

  for (;;) {
    gab_value key = d_gab_intern_ikey(&self->interned, index);
    d_status status = d_gab_intern_istatus(&self->interned, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_SHAPE(key)) {
      gab_obj_shape *shape_key = GAB_VAL_TO_SHAPE(key);
      if (shape_key->hash == hash &&
          shape_matches_keys(shape_key, keys, size, stride)) {
        return shape_key;
      }
    }

    index = (index + 1) & (self->interned.cap - 1);
  }
}

gab_value gab_get_type(gab_engine *gab, gab_type t) { return gab->types[t]; }
