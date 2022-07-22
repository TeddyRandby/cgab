#include "engine.h"
#include "module.h"
#include <dlfcn.h>

gab_engine *gab_create() {
  gab_engine *self = CREATE_STRUCT(gab_engine);

  self->constants = CREATE_STRUCT(d_u64);
  d_u64_create(self->constants, MODULE_CONSTANTS_MAX);

  self->imports = CREATE_STRUCT(d_u64);
  d_u64_create(self->imports, MODULE_CONSTANTS_MAX);

  self->modules = NULL;
  self->std = GAB_VAL_NULL();

  self->gc = (gab_gc){0};
  d_u64_create(&self->gc.roots, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->gc.queue, MODULE_CONSTANTS_MAX);

  self->vm = (gab_vm){0};

  return self;
}

gab_engine *gab_create_fork(gab_engine *parent) {
  gab_engine *self = CREATE_STRUCT(gab_engine);

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

  // Decrement the std and the constants.
  gab_engine_val_dref(self, self->std);
  gab_engine_collect(self);

  D_U64_FOR_INDEX_WITH_KEY_DO(self->imports, i) {
    gab_import *import = (gab_import *)d_u64_index_key(self->imports, i);
    gab_import_destroy(import);
  }

  D_U64_FOR_INDEX_WITH_KEY_DO(self->constants, i) {
    gab_value v = d_u64_index_key(self->constants, i);
    if (GAB_VAL_IS_OBJ(v))
      gab_obj_destroy(GAB_VAL_TO_OBJ(v), self);
  }

  d_u64_destroy(&self->gc.roots);
  d_u64_destroy(&self->gc.queue);
  d_u64_destroy(self->constants);
  d_u64_destroy(self->imports);
  DESTROY_STRUCT(self);
}

void gab_destroy_fork(gab_engine *self) {
  // Collect one last time.
  gab_engine_collect(self);

  d_u64_destroy(&self->gc.roots);
  d_u64_destroy(&self->gc.queue);

  DESTROY_STRUCT(self);
}

gab_import *gab_import_shared(void *shared, gab_value result) {
  gab_import *self = CREATE_STRUCT(gab_import);

  self->k = IMPORT_SHARED;
  self->shared = shared;
  self->cache = result;
  return self;
}

gab_import *gab_import_source(s_u8 *source, gab_value result) {
  gab_import *self = CREATE_STRUCT(gab_import);

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
    s_u8_destroy(self->source);
    break;
  }
  DESTROY_STRUCT(self);
  return;
}

gab_module *gab_engine_add_module(gab_engine *self, s_u8_ref name,
                                  s_u8_ref source) {

  gab_module *module = CREATE_STRUCT(gab_module);

  gab_module_create(module, name, source);

  module->engine = self;
  module->next = self->modules;
  self->modules = module;

  return module;
}

void gab_engine_add_import(gab_engine *self, gab_import *import,
                           s_u8_ref module) {
  d_u64_insert(self->imports, (uintptr_t)import, GAB_VAL_NULL(),
               s_u8_ref_hash(module));
}

u16 gab_engine_add_constant(gab_engine *self, gab_value value) {
  if (self->constants->capacity > MODULE_CONSTANTS_MAX) {
    failure(__FILE__, __LINE__, "Too many constants in this module.");
  }

  u64 hash = value;
  if (GAB_VAL_IS_STRING(value))
    hash = GAB_VAL_TO_STRING(value)->hash;
  else if (GAB_VAL_IS_SHAPE(value))
    hash = GAB_VAL_TO_SHAPE(value)->hash;

  d_u64_insert(self->constants, value, GAB_VAL_NULL(), hash);

  u64 val = d_u64_index_of(self->constants, value, hash);

  return val;
}

gab_obj_string *gab_engine_find_string(gab_engine *self, s_u8_ref str,
                                       u64 hash) {
  if (self->constants->size == 0)
    return NULL;

  u64 index = hash & (self->constants->capacity - 1);

  for (;;) {
    u64 key = v_u64_val_at(&self->constants->keys, index);
    u64 value = v_u64_val_at(&self->constants->values, index);

    if (GAB_VAL_IS_UNDEFINED(key)) {
      if (GAB_VAL_IS_NULL(value))
        return NULL;
    } else if (GAB_VAL_IS_STRING(key)) {
      gab_obj_string *str_key = GAB_VAL_TO_STRING(key);
      if (str_key->hash == hash &&
          s_u8_ref_match(str, gab_obj_string_ref(str_key))) {
        return str_key;
      }
    }

    index = (index + 1) & (self->constants->capacity - 1);
  }
}

static inline boolean shape_matches_keys(gab_obj_shape *self,
                                         gab_value values[], u64 size,
                                         u64 stride) {

  if (self->properties.size != size) {
    return false;
  }

  for (u64 i = 0; i < size; i++) {
    gab_value key = values[i * stride];
    if (!d_u64_has_key(&self->properties, key, key)) {
      return false;
    }
  }

  return true;
}

gab_obj_shape *gab_engine_find_shape(gab_engine *self, u64 size,
                                     gab_value values[size], u64 stride,
                                     u64 hash) {
  if (self->constants->size == 0)
    return NULL;

  u64 index = hash & (self->constants->capacity - 1);

  for (;;) {
    u64 key = v_u64_val_at(&self->constants->keys, index);
    u64 value = v_u64_val_at(&self->constants->values, index);

    if (GAB_VAL_IS_UNDEFINED(key)) {
      if (GAB_VAL_IS_NULL(value)) {
        return NULL;
      }
    } else if (GAB_VAL_IS_SHAPE(key)) {
      gab_obj_shape *shape_key = GAB_VAL_TO_SHAPE(key);
      if (shape_key->hash == hash &&
          shape_matches_keys(shape_key, values, size, stride)) {
        return shape_key;
      }
    }

    index = (index + 1) & (self->constants->capacity - 1);
  }
}
