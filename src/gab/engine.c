#include "include/engine.h"
#include "include/compiler.h"
#include "include/core.h"
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

  d_gab_intern_create(&gab->interned, 2);
  v_gab_module_create(&gab->modules, 8);
  v_gab_vm_create(&gab->vms, 8);
  gab_gc_create(&gab->gc);

  gab->flags = flags;

  return gab;
}

void gab_engine_dref_all(gab_engine *gab) {
  for (u64 i = 0; i < gab->interned.cap; i++) {
    if (d_gab_intern_iexists(&gab->interned, i)) {
      gab_value v = d_gab_intern_ikey(&gab->interned, i);
      gab_dref(gab, gab->vms.len - 1, v);
    }
  }
}

void gab_destroy(gab_engine *gab) {
  for (u32 i = 0; i < gab->modules.len; i++) {
    gab_module *m = v_gab_module_ref_at(&gab->modules, i);
    gab_module_dref_all(gab, m, 0);
    gab_module_destroy(m);
  }

  gab_engine_dref_all(gab);

  for (u32 i = 0; i < gab->vms.len; i++) {
    gab_vm *v = v_gab_vm_ref_at(&gab->vms, i);
    gab_gc_collect(&gab->gc, v);
  }

  d_gab_intern_destroy(&gab->interned);
  v_gab_module_destroy(&gab->modules);
  v_gab_vm_destroy(&gab->vms);

  gab_gc_destroy(&gab->gc);

  DESTROY(gab);
}

gab_value gab_compile_main(gab_engine *gab, s_i8 source) {
  return gab_bc_compile(gab, s_i8_cstr("__main__"), source);
}

gab_value gab_compile(gab_engine *gab, s_i8 name, s_i8 source) {
  return gab_bc_compile(gab, name, source);
}

i32 gab_spawn(gab_engine *gab) {
  gab_vm *vm = v_gab_vm_emplace(&gab->vms);
  gab_vm_create(vm);
  return gab->vms.len - 1;
}

gab_value gab_run(gab_engine *gab, i32 vm, gab_value main, u8 argc,
             gab_value argv[argc]) {

  return gab_vm_run(gab, vm, main, argc, argv);
};

gab_value gab_run_main(gab_engine *gab, i32 vm, gab_value main, gab_value globals) {
  return gab_run(gab, vm, main, 1, &globals);
};

void gab_dref(gab_engine *gab, i32 vm, gab_value value) {
  gab_vm *v = v_gab_vm_ref_at(&gab->vms, vm);
  gab_gc_dref(&gab->gc, v, value);
};

void gab_iref(gab_engine *gab, i32 vm, gab_value value) {
  gab_vm *v = v_gab_vm_ref_at(&gab->vms, vm);
  gab_gc_dref(&gab->gc, v, value);
};

gab_value gab_bundle_record(gab_engine *gab, u64 size, s_i8 keys[size],
                            gab_value values[size]) {
  gab_value value_keys[size];

  for (u64 i = 0; i < size; i++) {
    value_keys[i] = GAB_VAL_OBJ(gab_obj_string_create(gab, keys[i]));
  }

  gab_obj_shape *bundle_shape =
      gab_obj_shape_create(gab, NULL, size, 1, value_keys);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(bundle_shape, values, size, 1));

  return bundle;
}

gab_value gab_bundle_array(gab_engine *gab, u64 size, gab_value values[size]) {
  gab_obj_shape *bundle_shape = gab_obj_shape_create_array(gab, size);

  gab_value bundle =
      GAB_VAL_OBJ(gab_obj_record_create(bundle_shape, values, size, 1));

  return bundle;
}

gab_module *gab_engine_add_module(gab_engine *self, s_i8 name, s_i8 source) {
  gab_module *module = v_gab_module_emplace(&self->modules);

  gab_module_create(module, name, source);

  return module;
}

/**
 * Gab internal stuff
 */

u16 gab_engine_intern(gab_engine *self, gab_value value) {
  d_gab_intern_insert(&self->interned, value, 0);

  u64 val = d_gab_intern_index_of(&self->interned, value);

  return val;
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
