#include "module.h"
#include <stdio.h>

gab_engine *gab_engine_create() {
  gab_engine *self = CREATE_STRUCT(gab_engine);
  d_u64_create(&self->constants, MODULE_CONSTANTS_MAX);
  self->vm = NULL;
  self->modules = NULL;

  self->gc = gab_gc_create(self);
  self->std = GAB_VAL_NULL();

  return self;
}

void gab_engine_destroy(gab_engine *self) {

  // Walk the linked list of modules and clean them up.
  while (self->modules != NULL) {
    gab_module *next = self->modules->next;
    gab_module_destroy(self->modules);
    self->modules = next;
  }

  gab_gc_push_dec_if_obj_ref(self->gc, self->std);

  D_U64_FOR_INDEX_WITH_KEY_DO(&self->constants, i) {
    gab_gc_push_dec_if_obj_ref(self->gc, d_u64_index_key(&self->constants, i));
  }

  gab_gc_collect(self->gc);

  gab_gc_destroy(self->gc);

  d_u64_destroy(&self->constants);

  DESTROY_STRUCT(self);
}

void gab_engine_add_module(gab_engine *self, gab_module *module) {
  module->engine = self;
  module->next = self->modules;
  self->modules = module;
}

u16 gab_engine_add_constant(gab_engine *self, gab_value value) {
  if (self->constants.capacity > MODULE_CONSTANTS_MAX) {
    failure(__FILE__, __LINE__, "Too many constants in this module.");
  }

  u64 hash = value;
  if (GAB_VAL_IS_STRING(value))
    hash = GAB_VAL_TO_STRING(value)->hash;
  else if (GAB_VAL_IS_SHAPE(value))
    hash = GAB_VAL_TO_SHAPE(value)->hash;

  d_u64_insert(&self->constants, value, GAB_VAL_NULL(), hash);

  u64 val = d_u64_index_of(&self->constants, value, hash);

  return val;
}

gab_obj_string *gab_engine_find_string(gab_engine *self, s_u8_ref str,
                                       u64 hash) {
  if (self->constants.size == 0)
    return NULL;

  u64 index = hash & (self->constants.capacity - 1);

  for (;;) {
    u64 key = v_u64_val_at(&self->constants.keys, index);
    u64 value = v_u64_val_at(&self->constants.values, index);

    if (GAB_VAL_IS_UNDEFINED(key)) {
      if (GAB_VAL_IS_NULL(value))
        return NULL;
    } else if (GAB_VAL_IS_STRING(key)) {
      gab_obj_string *str_key = GAB_VAL_TO_STRING(key);
      if (str_key->hash == hash &&
          s_u8_ref_match_lit(str, (char *)&str_key->data[0])) {
        return str_key;
      }
    }

    index = (index + 1) & (self->constants.capacity - 1);
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
  if (self->constants.size == 0)
    return NULL;

  u64 index = hash & (self->constants.capacity - 1);

  for (;;) {
    u64 key = v_u64_val_at(&self->constants.keys, index);
    u64 value = v_u64_val_at(&self->constants.values, index);

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

    index = (index + 1) & (self->constants.capacity - 1);
  }
}

gab_module *gab_module_create(s_u8_ref name, s_u8 *source) {
  gab_module *self = CREATE_STRUCT(gab_module);

  self->name = name;
  self->source = source;
  self->ntop_level = 0;
  v_u8_create(&self->bytecode);
  v_u8_create(&self->tokens);
  v_u64_create(&self->lines);

  self->next = NULL;
  return self;
}

void gab_module_destroy(gab_module *self) {
  s_u8_destroy(self->source);

  v_u8_destroy(&self->bytecode);
  v_u8_destroy(&self->tokens);
  v_u64_destroy(&self->lines);
  v_s_u8_ref_destroy(self->source_lines);

  DESTROY_STRUCT(self->source_lines);
  DESTROY_STRUCT(self);
}

void gab_module_push_byte(gab_module *self, u8 b) {
  v_u8_push(&self->bytecode, b);
}
