#include "include/engine.h"
#include "include/gc.h"
#include "include/compiler.h"
#include "include/vm.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

boolean gab_result_ok(gab_result self) { return self.k >= 0; };

gab_value gab_result_value(gab_result self) { return self.result; }

gab_engine *gab_create(a_i8* error) {
  gab_engine *self = NEW(gab_engine);

  self->constants = NEW(d_gab_constant);
  d_gab_constant_create(self->constants, MODULE_CONSTANTS_MAX);

  self->tags = NEW(d_gab_container_tag);
  d_gab_container_tag_create(self->tags, MODULE_CONSTANTS_MAX);

  self->modules = NULL;
  self->std = GAB_VAL_NULL();

  gab_bc_create(&self->bc);
  gab_vm_create(&self->vm);
  gab_gc_create(&self->gc);

  self->error = error;

  self->owning = true;

  pthread_mutexattr_t attr;
  pthread_mutex_init(&self->lock, &attr);

  return self;
}

gab_engine *gab_fork(gab_engine *parent) {
  gab_engine *self = NEW(gab_engine);

  self->constants = parent->constants;
  self->modules = parent->modules;
  self->std = parent->std;

  self->gc = (gab_gc){0};
  d_u64_create(&self->gc.roots, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->gc.queue, MODULE_CONSTANTS_MAX);

  self->vm = (gab_vm){0};

  self->owning = false;

  pthread_mutexattr_t attr;
  pthread_mutex_init(&self->lock, &attr);
  return self;
};

void gab_destroy(gab_engine *self) {
  if (self->owning) {
    // Walk the linked list of modules and clean them up.
    while (self->modules != NULL) {
      gab_module *next = self->modules->next;
      gab_module_destroy(self->modules);
      self->modules = next;
    }

    for (u64 i = 0; i < self->constants->cap; i++) {
      if (d_gab_constant_iexists(self->constants, i)) {
        gab_value v = d_gab_constant_ikey(self->constants, i);
        gab_dref(self, v);
      }
    }
  }

  gab_collect(self);

  d_u64_destroy(&self->gc.roots);
  d_u64_destroy(&self->gc.queue);

  pthread_mutex_destroy(&self->lock);

  if (self->owning) {
    d_gab_constant_destroy(self->constants);
    d_gab_container_tag_destroy(self->tags);
    DESTROY(self->constants);
    DESTROY(self->tags);
  }

  DESTROY(self);
}

gab_module *gab_add_module(gab_engine *self, s_i8 name, s_i8 source) {

  gab_module *module = NEW(gab_module);

  gab_module_create(module, name, source);

  module->engine = self;
  module->next = self->modules;
  self->modules = module;

  return module;
}

boolean gab_add_container_tag(gab_engine *self, gab_value tag,
                                  gab_obj_container_cb destructor) {
  if (d_gab_container_tag_exists(self->tags, tag)) {
    return false;
  }

  return d_gab_container_tag_insert(self->tags, tag, destructor);
}

u16 gab_add_constant(gab_engine *self, gab_value value) {
  if (self->constants->cap > MODULE_CONSTANTS_MAX) {
    fprintf(stderr, "Too many constants\n");
    exit(1);
  }

  d_gab_constant_insert(self->constants, value, 0);

  u64 val = d_gab_constant_index_of(self->constants, value);

  return val;
}

gab_obj_string *gab_find_string(gab_engine *self, s_i8 str, u64 hash) {
  if (self->constants->len == 0)
    return NULL;

  u64 index = hash & (self->constants->cap - 1);

  for (;;) {
    gab_value key = d_gab_constant_ikey(self->constants, index);
    d_status status = d_gab_constant_istatus(self->constants, index);

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

gab_obj_shape *gab_find_shape(gab_engine *self, u64 size, u64 stride,
                                     u64 hash, gab_value keys[size]) {
  if (self->constants->len == 0)
    return NULL;

  u64 index = hash & (self->constants->cap - 1);

  for (;;) {
    gab_value key = d_gab_constant_ikey(self->constants, index);
    d_status status = d_gab_constant_istatus(self->constants, index);

    if (status != D_FULL) {
      return NULL;
    } else if (GAB_VAL_IS_SHAPE(key)) {
      gab_obj_shape *shape_key = GAB_VAL_TO_SHAPE(key);
      if (shape_key->hash == hash &&
          shape_matches_keys(shape_key, keys, size, stride)) {
        return shape_key;
      }
    }

    index = (index + 1) & (self->constants->cap - 1);
  }
}



