#include "include/gc.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>

#if GAB_DEBUG_GC
boolean debug_collect = true;
#endif

void gab_obj_iref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_obj *obj) {
  gc->increments[gc->increment_count++] = obj;
  if (gc->increment_count == INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
  }
}

void gab_obj_dref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_obj *obj) {
  if (gc->decrement_count == INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
  }
  gc->decrements[gc->decrement_count++] = obj;
}

#if GAB_DEBUG_GC

void __gab_gc_iref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_value obj,
                   const char *file, i32 line) {
  if (GAB_VAL_IS_OBJ(obj)) {
    gab_obj_iref(gab, vm, gc, GAB_VAL_TO_OBJ(obj));
    v_rc_update_push(&gc->tracked_increments, (rc_update){
                                                  .val = GAB_VAL_TO_OBJ(obj),
                                                  .file = file,
                                                  .line = line,
                                              });
  }

  if (debug_collect)
    gab_gc_collect(gab, vm, gc);
}

void __gab_gc_dref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_value obj,
                   const char *file, i32 line) {

  if (debug_collect)
    gab_gc_collect(gab, vm, gc);

  if (GAB_VAL_IS_OBJ(obj)) {
    gab_obj_dref(gab, vm, gc, GAB_VAL_TO_OBJ(obj));
    v_rc_update_push(&gc->tracked_decrements, (rc_update){
                                                  .val = GAB_VAL_TO_OBJ(obj),
                                                  .file = file,
                                                  .line = line,
                                              });
  }
}

static inline void dump_rcs_for(gab_gc *gc, gab_obj *val) {

  for (u64 j = 0; j < gc->tracked_increments.len; j++) {
    rc_update *u = v_rc_update_ref_at(&gc->tracked_increments, j);
    if (u->val == val) {
      fprintf(stdout, "+1 %s:%i.\n", u->file, u->line);
    }
  }

  for (u64 j = 0; j < gc->tracked_decrements.len; j++) {
    rc_update *u = v_rc_update_ref_at(&gc->tracked_decrements, j);
    if (u->val == val) {
      fprintf(stdout, "-1 %s:%i.\n", u->file, u->line);
    }
  }
}

#else

void gab_gc_iref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj))
    gab_obj_iref(gab, vm, gc, GAB_VAL_TO_OBJ(obj));
}

void gab_gc_dref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj))
    gab_obj_dref(gab, vm, gc, GAB_VAL_TO_OBJ(obj));
}

#endif

void gab_gc_create(gab_gc *self) {
  self->decrement_count = 0;
  self->increment_count = 0;
  self->root_count = 0;

  d_gc_set_create(&self->queue, MODULE_CONSTANTS_MAX);

#if GAB_DEBUG_GC
  v_rc_update_create(&self->tracked_decrements, MODULE_CONSTANTS_MAX);
  v_rc_update_create(&self->tracked_increments, MODULE_CONSTANTS_MAX);
  d_rc_tracker_create(&self->tracked_values, MODULE_CONSTANTS_MAX);
#endif
};

void gab_gc_destroy(gab_gc *self) {
  d_gc_set_destroy(&self->queue);

#if GAB_DEBUG_GC

  fprintf(stdout, "Checking remaining objects...\n");
  for (u64 i = 0; i < self->tracked_values.cap; i++) {
    if (d_rc_tracker_iexists(&self->tracked_values, i)) {
      gab_obj *k = d_rc_tracker_ikey(&self->tracked_values, i);
      i32 v = d_rc_tracker_ival(&self->tracked_values, i);
      if (v > 0 && v < 200) {
        gab_val_dump(GAB_VAL_OBJ(k));
        fprintf(stdout, " had %i remaining references.\n", v);
        dump_rcs_for(self, k);
      }
    }
  }
  fprintf(stdout, "Done.\n");

  v_rc_update_destroy(&self->tracked_decrements);
  v_rc_update_destroy(&self->tracked_increments);
  d_rc_tracker_destroy(&self->tracked_values);
#endif
}

void *gab_reallocate(void *loc, u64 old_count, u64 new_count) {
  if (new_count == 0) {
    free(loc);
    return NULL;
  }

  void *new_ptr = realloc(loc, new_count);

  if (!new_ptr) {
    exit(1);
  }

  return new_ptr;
}

static inline void queue_destroy(gab_gc *gc, gab_obj *obj) {
  d_gc_set_insert(&gc->queue, obj, 0);
}

static inline void cleanup(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  for (i32 i = 0; i < gc->queue.cap; i++) {
    if (d_gc_set_iexists(&gc->queue, i)) {
      d_gc_set_iremove(&gc->queue, i);

      gab_obj *key = d_gc_set_ikey(&gc->queue, i);

#if GAB_LOG_GC
      printf("Destroying: ");
      gab_val_dump(GAB_VAL_OBJ(key));
      printf("\n");
#if GAB_DEBUG_GC
      dump_rcs_for(gc, key);
#endif
#endif

      gab_obj_destroy(gab, vm, key);
    }
  }
}

static inline void push_root(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                             gab_obj *obj) {
  if (gc->root_count >= INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
  }

  d_gc_set_insert(&gc->roots, obj, 0);
}

static inline void obj_possible_root(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                     gab_obj *obj) {
  if (!GAB_OBJ_IS_PURPLE(obj)) {
    GAB_OBJ_PURPLE(obj);
    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      GAB_OBJ_BUFFERED(obj);
      push_root(gab, vm, gc, obj);
    }
  }
}

typedef void (*child_and_gc_iter)(gab_gc *gc, gab_obj *obj);
static inline void for_child_and_gc_do(gab_gc *gc, gab_obj *obj,
                                       child_and_gc_iter fnc) {
  switch (obj->kind) {
  case TYPE_STRING:
  case TYPE_PROTOTYPE:
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_CONTAINER:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_BOOLEAN:
  case GAB_NTYPES:
    break;
  case TYPE_EFFECT: {
    gab_obj_effect *eff = (gab_obj_effect *)obj;
    for (u8 i = 0; i < eff->len; i++) {
      if (GAB_VAL_IS_OBJ(eff->frame[i]))
        fnc(gc, GAB_VAL_TO_OBJ(eff->frame[i]));
    }
    break;
  }
  case (TYPE_CLOSURE): {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->nupvalues; i++) {
      if (GAB_VAL_IS_OBJ(closure->upvalues[i]))
        fnc(gc, GAB_VAL_TO_OBJ(closure->upvalues[i]));
    }
    break;
  }
  case (TYPE_MESSAGE): {
    gab_obj_message *func = (gab_obj_message *)obj;
    for (u64 i = 0; i < func->specs.cap; i++) {
      if (d_specs_iexists(&func->specs, i)) {
        gab_value s = d_specs_ival(&func->specs, i);
        if (GAB_VAL_IS_OBJ(s)) {
          fnc(gc, GAB_VAL_TO_OBJ(s));
        }
      }
    }
    break;
  }
  case (TYPE_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(gc, GAB_VAL_TO_OBJ(*upvalue->data));
    };
    break;
  }
  case TYPE_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.len; i++) {
      if (GAB_VAL_IS_OBJ(shape->keys[i])) {
        fnc(gc, GAB_VAL_TO_OBJ(shape->keys[i]));
      }
    }
    break;
  }
  case (TYPE_RECORD): {
    gab_obj_record *object = (gab_obj_record *)obj;
    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.len; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          fnc(gc, GAB_VAL_TO_OBJ(object->dynamic_values.data[i]));
        }
      }
    } else {
      for (u64 i = 0; i < object->static_len; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          fnc(gc, GAB_VAL_TO_OBJ(object->static_values[i]));
        }
      }
    }
    break;
  }
  }
}

typedef void (*child_iter)(gab_obj *obj);
static inline void for_child_do(gab_obj *obj, child_iter fnc) {
  switch (obj->kind) {
  case TYPE_STRING:
  case TYPE_PROTOTYPE:
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_CONTAINER:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_BOOLEAN:
  case GAB_NTYPES:
    break;

  case TYPE_EFFECT: {
    gab_obj_effect *eff = (gab_obj_effect *)obj;
    for (u8 i = 0; i < eff->len; i++) {
      if (GAB_VAL_IS_OBJ(eff->frame[i]))
        fnc(GAB_VAL_TO_OBJ(eff->frame[i]));
    }
    break;
  }
  case (TYPE_CLOSURE): {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->nupvalues; i++) {
      if (GAB_VAL_IS_OBJ(closure->upvalues[i])) {
        fnc(GAB_VAL_TO_OBJ(closure->upvalues[i]));
      }
    }
    break;
  }
  case (TYPE_MESSAGE): {
    gab_obj_message *func = (gab_obj_message *)obj;
    for (u64 i = 0; i < func->specs.cap; i++) {
      if (d_specs_iexists(&func->specs, i)) {
        gab_value s = d_specs_ival(&func->specs, i);
        if (GAB_VAL_IS_OBJ(s)) {
          fnc(GAB_VAL_TO_OBJ(s));
        }
      }
    }
    break;
  }
  case (TYPE_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(GAB_VAL_TO_OBJ(*upvalue->data));
    };
    break;
  }
  case TYPE_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.len; i++) {
      if (GAB_VAL_IS_OBJ(shape->keys[i])) {
        fnc(GAB_VAL_TO_OBJ(shape->keys[i]));
      }
    }
    break;
  }
  case (TYPE_RECORD): {
    gab_obj_record *object = (gab_obj_record *)obj;

    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.len; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          fnc(GAB_VAL_TO_OBJ(object->dynamic_values.data[i]));
        }
      }
    } else {
      for (u64 i = 0; i < object->static_len; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          fnc(GAB_VAL_TO_OBJ(object->static_values[i]));
        }
      }
    }

    break;
  }
  }
}

// Forward declarations - these functions recurse into each other.
static inline void dec_obj_ref(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                               gab_obj *obj);
static inline void dec_if_obj_ref(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                  gab_value val);
static inline void dec_child_refs(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                  gab_obj *obj) {
  switch (obj->kind) {
  case TYPE_STRING:
  case TYPE_PROTOTYPE:
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_CONTAINER:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_BOOLEAN:
  case GAB_NTYPES:
    break;
  case TYPE_CLOSURE: {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->nupvalues; i++) {
      dec_if_obj_ref(gab, vm, gc, closure->upvalues[i]);
    }
    break;
  }
  case TYPE_EFFECT: {
    gab_obj_effect *eff = (gab_obj_effect *)obj;
    for (u8 i = 0; i < eff->len; i++) {
      dec_if_obj_ref(gab, vm, gc, eff->frame[i]);
    }
    break;
  }
  case (TYPE_MESSAGE): {
    gab_obj_message *func = (gab_obj_message *)obj;
    for (u64 i = 0; i < func->specs.cap; i++) {
      if (d_specs_iexists(&func->specs, i)) {
        gab_value r = d_specs_ikey(&func->specs, i);
        dec_if_obj_ref(gab, vm, gc, r);

        gab_value s = d_specs_ival(&func->specs, i);
        dec_if_obj_ref(gab, vm, gc, s);
      }
    }
    break;
  }
  case TYPE_UPVALUE: {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    dec_if_obj_ref(gab, vm, gc, upvalue->closed);
    break;
  }
  case TYPE_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.len; i++) {
      dec_if_obj_ref(gab, vm, gc, shape->keys[i]);
    }
    break;
  }
  case TYPE_RECORD: {
    gab_obj_record *object = (gab_obj_record *)obj;
    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.len; i++) {
        dec_if_obj_ref(gab, vm, gc, object->dynamic_values.data[i]);
      }
    } else {
      for (u64 i = 0; i < object->static_len; i++) {
        dec_if_obj_ref(gab, vm, gc, object->static_values[i]);
      }
    }
    break;
  }
  }
}

static inline void dec_obj_ref(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                               gab_obj *obj) {
#if GAB_DEBUG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references - 1);
#endif
  if (--obj->references == 0) {
    dec_child_refs(gab, vm, gc, obj);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      queue_destroy(gc, obj);
    }
  } else {
    obj_possible_root(gab, vm, gc, obj);
  }
}

static inline void dec_if_obj_ref(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                  gab_value val) {
  if (GAB_VAL_IS_OBJ(val)) {
    dec_obj_ref(gab, vm, gc, GAB_VAL_TO_OBJ(val));
  }
}

static inline void inc_obj_ref(gab_gc *gc, gab_obj *obj) {
#if GAB_DEBUG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references + 1);
#endif
  obj->references++;
  // This object cannot be garbage.
  GAB_OBJ_BLACK(obj);
}

static inline void inc_if_obj_ref(gab_gc *gc, gab_value val) {
  if (GAB_VAL_IS_OBJ(val)) {
    inc_obj_ref(gc, GAB_VAL_TO_OBJ(val));
  }
}

static inline void increment_stack(gab_gc *gc, gab_vm *vm) {
  gab_value *tracker = vm->top;
  while (--tracker >= vm->stack) {
#if GAB_DEBUG_GC
    if (GAB_VAL_IS_OBJ(*tracker)) {
      v_rc_update_push(&gc->tracked_increments,
                       (rc_update){
                           .val = GAB_VAL_TO_OBJ(*tracker),
                           .file = __FILE__,
                           .line = __LINE__,
                       });
    }
#endif
    inc_if_obj_ref(gc, *tracker);
  }
}

static inline void decrement_stack(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
#if GAB_DEBUG_GC
  debug_collect = false;
#endif
  gab_value *tracker = vm->top;
  while (--tracker >= vm->stack) {
    gab_gc_dref(gab, vm, gc, *tracker);
  }
#if GAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void process_increments(gab_gc *gc) {
  while (gc->increment_count > 0) {
    gc->increment_count--;
    inc_obj_ref(gc, gc->increments[gc->increment_count]);
  }
}

static inline void process_decrements(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  while (gc->decrement_count > 0) {
    gc->decrement_count--;
    dec_obj_ref(gab, vm, gc, gc->decrements[gc->decrement_count]);
  }
}

static inline void mark_gray(gab_obj *obj);

static inline void dec_and_mark_gray(gab_obj *child) {
  if (child) {
    child->references -= 1;
    if (!GAB_OBJ_IS_GRAY(child)) {
      mark_gray(child);
    }
  }
}

static inline void mark_gray(gab_obj *obj) {
  if (!GAB_OBJ_IS_GRAY(obj)) {
    GAB_OBJ_GRAY(obj);
    for_child_do(obj, &dec_and_mark_gray);
  }
}

static inline void mark_roots(gab_gc *gc) {
  for (i32 i = 0; i < gc->roots.cap; i++) {
    if (d_gc_set_iexists(&gc->roots, i)) {
      gab_obj *obj = d_gc_set_ikey(&gc->roots, i);

      if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
        mark_gray(obj);
      } else {
        GAB_OBJ_NOT_BUFFERED(obj);

        d_gc_set_iremove(&gc->roots, i);
        // SHould this be a not?
        if (GAB_OBJ_IS_BLACK(obj) && obj->references == 0) {
          queue_destroy(gc, obj);
        }
      }
    }
  }
}

static inline void scan_root_black(gab_obj *obj);
static inline void inc_and_scan_black(gab_obj *child) {
  child->references++;
  if (!GAB_OBJ_IS_BLACK(child))
    scan_root_black(child);
}

static inline void scan_root_black(gab_obj *obj) {
  GAB_OBJ_BLACK(obj);
  for_child_do(obj, &inc_and_scan_black);
}

static inline void scan_root(gab_obj *obj) {
  if (GAB_OBJ_IS_GRAY(obj)) {
    if (obj->references > 0) {
      scan_root_black(obj);
    } else {
      GAB_OBJ_WHITE(obj);
      for_child_do(obj, &scan_root);
    }
  }
}

static inline void scan_roots(gab_gc *gc) {
  for (i32 i = 0; i < gc->roots.cap; i++) {
    if (d_gc_set_iexists(&gc->roots, i)) {
      gab_obj *obj = d_gc_set_ikey(&gc->roots, i);
      scan_root(obj);
    }
  }
}

static inline void collect_white(gab_gc *gc, gab_obj *obj) {
  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

    for_child_and_gc_do(gc, obj, collect_white);
    queue_destroy(gc, obj);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(gab_gc *gc) {
  for (u64 i = 0; i < gc->roots.cap; i++) {
    if (d_gc_set_iexists(&gc->roots, i)) {
      gab_obj *obj = d_gc_set_ikey(&gc->roots, i);
      collect_white(gc, obj);
    }
  }
  gc->root_count = 0;
}

void collect_cycles(gab_gc *gc) {
  mark_roots(gc);
  scan_roots(gc);
  collect_roots(gc);
}

void gab_gc_collect(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  d_gc_set_create(&gc->roots, MODULE_CONSTANTS_MAX);

  if (vm != NULL)
    increment_stack(gc, vm);

  process_increments(gc);

  process_decrements(gab, vm, gc);

  if (vm != NULL)
    decrement_stack(gab, vm, gc);

  collect_cycles(gc);

  cleanup(gab, vm, gc);

  d_gc_set_destroy(&gc->roots);
}
