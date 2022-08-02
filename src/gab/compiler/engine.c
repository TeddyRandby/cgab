#include "engine.h"
#include "module.h"
#include <dlfcn.h>

gab_engine *gab_create() {
  gab_engine *self = NEW(gab_engine);

  self->constants = NEW(d_gab_value);
  d_gab_value_create(self->constants, MODULE_CONSTANTS_MAX);

  self->imports = NEW(d_s_i8);
  d_s_i8_create(self->imports, MODULE_CONSTANTS_MAX);

  self->modules = NULL;
  self->std = GAB_VAL_NULL();

  self->gc = (gab_gc){0};
  d_u64_create(&self->gc.roots, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->gc.queue, MODULE_CONSTANTS_MAX);

  self->vm = (gab_vm){0};

  return self;
}

gab_engine *gab_create_fork(gab_engine *parent) {
  gab_engine *self = NEW(gab_engine);

  self->constants = parent->constants;
  self->modules = parent->modules;
  self->imports = parent->imports;
  self->std = parent->std;

  self->gc = (gab_gc){0};
  d_u64_create(&self->gc.roots, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->gc.queue, MODULE_CONSTANTS_MAX);

  self->vm = (gab_vm){0};
  return self;
};

void gab_destroy(gab_engine *self) {
  // Walk the linked list of modules and clean them up.
  while (self->modules != NULL) {
    gab_module *next = self->modules->next;
    gab_module_destroy(self->modules);
    self->modules = next;
  }

  for (i32 i = 0; i < self->imports->cap; i++) {
    if (d_s_i8_iexists(self->imports, i)) {
      gab_import *import = (gab_import *)d_s_i8_ival(self->imports, i);
      gab_import_destroy(import);
    }
  }

  for (u64 i = 0; i < self->constants->cap; i++) {
    if (d_gab_value_iexists(self->constants, i)) {
      gab_value v = d_gab_value_ikey(self->constants, i);
      gab_engine_val_dref(self, v);
    }
  }

  // Decrement the std and the constants.
  gab_engine_val_dref(self, self->std);
  gab_engine_collect(self);

  d_u64_destroy(&self->gc.roots);
  d_u64_destroy(&self->gc.queue);

  d_gab_value_destroy(self->constants);
  d_s_i8_destroy(self->imports);

  DESTROY(self);
}

void gab_destroy_fork(gab_engine *self) {
  // Collect one last time.
  gab_engine_collect(self);

  d_u64_destroy(&self->gc.roots);
  d_u64_destroy(&self->gc.queue);

  DESTROY(self);
}

gab_import *gab_import_shared(void *shared, gab_value result) {
  gab_import *self = NEW(gab_import);

  self->k = IMPORT_SHARED;
  self->shared = shared;
  self->cache = result;
  return self;
}

gab_import *gab_import_source(a_i8 *source, gab_value result) {
  gab_import *self = NEW(gab_import);

  self->k = IMPORT_SOURCE;
  self->source = source;
  self->cache = result;
  return self;
}

void gab_import_destroy(gab_import *self) {
  switch (self->k) {
  case IMPORT_SHARED:
    dlclose(self->shared);
    break;
  case IMPORT_SOURCE:
    a_i8_destroy(self->source);
    break;
  }
  DESTROY(self);
  return;
}

gab_module *gab_engine_add_module(gab_engine *self, s_i8 name, s_i8 source) {

  gab_module *module = NEW(gab_module);

  gab_module_create(module, name, source);

  module->engine = self;
  module->next = self->modules;
  self->modules = module;

  return module;
}

void gab_engine_add_import(gab_engine *self, gab_import *import, s_i8 module) {
  d_s_i8_insert(self->imports, module, import);
}

u16 gab_engine_add_constant(gab_engine *self, gab_value value) {
  if (self->constants->cap > MODULE_CONSTANTS_MAX) {
    fprintf(stderr, "Too many constants\n");
    exit(1);
  }

  d_gab_value_insert(self->constants, value, 0);

  u64 val = d_gab_value_index_of(self->constants, value);

  return val;
}

gab_obj_string *gab_engine_find_string(gab_engine *self, s_i8 str, u64 hash) {
  if (self->constants->len == 0)
    return NULL;

  u64 index = hash & (self->constants->cap - 1);

  for (;;) {
    gab_value key = d_gab_value_ikey(self->constants, index);
    d_status status = d_gab_value_istatus(self->constants, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_STRING(key)) {
      gab_obj_string *str_key = GAB_VAL_TO_STRING(key);
      if (str_key->hash == hash &&
          s_i8_match(str, gab_obj_string_ref(str_key))) {
        return str_key;
      }
    }

    index = (index + 1) & (self->constants->cap - 1);
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

gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size,
                                     gab_value values[size], u64 stride,
                                     u64 hash) {
  if (self->constants->len == 0)
    return NULL;

  u64 index = hash & (self->constants->cap - 1);

  for (;;) {
    gab_value key = d_gab_value_ikey(self->constants, index);
    d_status status = d_gab_value_istatus(self->constants, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_SHAPE(key)) {
      gab_obj_shape *shape_key = GAB_VAL_TO_SHAPE(key);
      if (shape_key->hash == hash &&
          shape_matches_keys(shape_key, values, size, stride)) {
        return shape_key;
      }
    }

    index = (index + 1) & (self->constants->cap - 1);
  }
}
