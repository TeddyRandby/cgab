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

  d_strings_create(&gab->interned_strings, INTERN_INITIAL_CAP);
  d_shapes_create(&gab->interned_shapes, INTERN_INITIAL_CAP);
  d_functions_create(&gab->interned_functions, INTERN_INITIAL_CAP);

  gab_gc_create(&gab->gc);

  gab->flags = flags;

  gab->types[TYPE_ANY] = GAB_SYMBOL("any");
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

void gab_cleanup(gab_engine *gab) {
  if (gab == NULL)
    return;

  for (u64 i = 0; i < gab->interned_strings.cap; i++) {
    if (d_strings_iexists(&gab->interned_strings, i)) {
      gab_obj_string *v = d_strings_ikey(&gab->interned_strings, i);
      gab_dref(gab, NULL, GAB_VAL_OBJ(v));
    }
  }

  for (u64 i = 0; i < gab->interned_shapes.cap; i++) {
    if (d_shapes_iexists(&gab->interned_shapes, i)) {
      gab_obj_shape *v = d_shapes_ikey(&gab->interned_shapes, i);
      gab_dref(gab, NULL, GAB_VAL_OBJ(v));
    }
  }

  for (u64 i = 0; i < gab->interned_functions.cap; i++) {
    if (d_functions_iexists(&gab->interned_functions, i)) {
      gab_obj_function *v = d_functions_ikey(&gab->interned_functions, i);
      gab_dref(gab, NULL, GAB_VAL_OBJ(v));
    }
  }

  for (u8 i = 0; i < GAB_NTYPES; i++) {
    gab_dref(gab, NULL, gab->types[i]);
  }
}

void gab_destroy(gab_engine *gab) {
  if (gab == NULL)
    return;

  d_strings_destroy(&gab->interned_strings);
  d_shapes_destroy(&gab->interned_shapes);
  d_functions_destroy(&gab->interned_functions);

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

boolean gab_specialize(gab_engine *gab, s_i8 name, gab_value receiver,
                    gab_value specialization) {
  gab_obj_function *f = gab_obj_function_create(gab, name);

  return gab_obj_function_set(f, receiver, specialization);
}

/**
 * Gab internal stuff
 */

i32 gab_engine_intern(gab_engine *self, gab_value value) {
  if (GAB_VAL_IS_STRING(value)) {
    gab_obj_string *k = GAB_VAL_TO_STRING(value);

    d_strings_insert(&self->interned_strings, k, 0);

    return d_strings_index_of(&self->interned_strings, k);
  }

  if (GAB_VAL_IS_SHAPE(value)) {
    gab_obj_shape *k = GAB_VAL_TO_SHAPE(value);

    d_shapes_insert(&self->interned_shapes, k, 0);

    return d_shapes_index_of(&self->interned_shapes, k);
  }

  if (GAB_VAL_IS_FUNCTION(value)) {
    gab_obj_function *k = GAB_VAL_TO_FUNCTION(value);

    d_functions_insert(&self->interned_functions, k, 0);

    return d_functions_index_of(&self->interned_functions, k);
  }

  return -1;
}

gab_obj_function *gab_engine_find_function(gab_engine *self, s_i8 name,
                                           u64 hash) {
  if (self->interned_functions.len == 0)
    return NULL;

  u64 index = hash & (self->interned_functions.cap - 1);

  for (;;) {
    gab_obj_function *key = d_functions_ikey(&self->interned_functions, index);
    d_status status = d_functions_istatus(&self->interned_functions, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && s_i8_match(name, key->name)) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_functions.cap - 1);
  }
}

gab_obj_string *gab_engine_find_string(gab_engine *self, s_i8 str, u64 hash) {
  if (self->interned_strings.len == 0)
    return NULL;

  u64 index = hash & (self->interned_strings.cap - 1);

  for (;;) {
    gab_obj_string *key = d_strings_ikey(&self->interned_strings, index);
    d_status status = d_strings_istatus(&self->interned_strings, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && s_i8_match(str, gab_obj_string_ref(key))) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_strings.cap - 1);
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
  if (self->interned_shapes.len == 0)
    return NULL;

  u64 index = hash & (self->interned_shapes.cap - 1);

  for (;;) {
    gab_obj_shape *key = d_shapes_ikey(&self->interned_shapes, index);
    d_status status = d_shapes_istatus(&self->interned_shapes, index);

    if (status != D_FULL) {
      return NULL;
    } else {
      if (key->hash == hash && shape_matches_keys(key, keys, size, stride)) {
        return key;
      }
    }

    index = (index + 1) & (self->interned_shapes.cap - 1);
  }
}

gab_value gab_get_type(gab_engine *gab, gab_type t) { return gab->types[t]; }
