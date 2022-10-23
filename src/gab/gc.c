#include "include/gc.h"
#include "include/engine.h"
#include "include/object.h"

void gab_obj_iref(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  gc->increments[gc->increment_count++] = obj;
#if GAB_DEBUG_GC
  gab_gc_collect(gc, vm);
#endif
  if (gc->increment_count == INC_DEC_MAX) {
    gab_gc_collect(gc, vm);
  }
}

void gab_obj_dref(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  gc->decrements[gc->decrement_count++] = obj;
  if (gc->decrement_count == INC_DEC_MAX) {
    gab_gc_collect(gc, vm);
  }
}

void gab_gc_iref(gab_gc *gc, gab_vm *vm, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj))
    gab_obj_iref(gc, vm, GAB_VAL_TO_OBJ(obj));
}

void gab_gc_dref(gab_gc *gc, gab_vm *vm, gab_value obj) {
  if (GAB_VAL_IS_OBJ(obj))
    gab_obj_dref(gc, vm, GAB_VAL_TO_OBJ(obj));
}

void gab_gc_create(gab_gc *self) {
  self->decrement_count = 0;
  self->increment_count = 0;
  self->root_count = 0;

  d_u64_create(&self->roots, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->queue, MODULE_CONSTANTS_MAX);
};

void gab_gc_destroy(gab_gc *self) {
  d_u64_destroy(&self->roots);
  d_u64_destroy(&self->queue);
}

void *gab_reallocate(void *loc, u64 old_count, u64 new_count) {
  if (new_count == 0) {
    free(loc);
    return NULL;
  }

  void *new_ptr = realloc(loc, new_count);

  if (!new_ptr) {
    if (!new_ptr) {
      exit(1);
    }
  }

  return new_ptr;
}

static inline void queue_destroy(gab_gc *gc, gab_obj *obj) {
  d_u64_insert(&gc->queue, GAB_VAL_OBJ(obj), GAB_VAL_NULL());
}

static inline void cleanup(gab_gc *gc) {
  for (i32 i = 0; i < gc->queue.cap; i++) {
    if (d_u64_iexists(&gc->queue, i)) {
      gab_value key = d_u64_ikey(&gc->queue, i);
      gab_obj_destroy(GAB_VAL_TO_OBJ(key));
      d_u64_iremove(&gc->queue, i);
    }
  }
}

static inline void push_root(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  if (gc->root_count >= INC_DEC_MAX) {
    gab_gc_collect(gc, vm);
  }

  d_u64_insert(&gc->roots, GAB_VAL_OBJ(obj), GAB_VAL_NULL());
}

static inline void obj_possible_root(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  if (!GAB_OBJ_IS_PURPLE(obj)) {
    GAB_OBJ_PURPLE(obj);
    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      GAB_OBJ_BUFFERED(obj);
      push_root(gc, vm, obj);
    }
  }
}

typedef void (*child_and_gc_iter)(gab_gc *gc, gab_obj *obj);
static inline void for_child_and_gc_do(gab_gc *gc, gab_obj *obj,
                                       child_and_gc_iter fnc) {
  gab_obj *child;
  switch (obj->kind) {
  case (OBJECT_CLOSURE): {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->func->nupvalues; i++) {
      child = (gab_obj *)closure->upvalues[i];
      fnc(gc, child);
    }
    return;
  }
  case (OBJECT_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(gc, GAB_VAL_TO_OBJ(*upvalue->data));
    };
    return;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.len; i++) {
      if (GAB_VAL_IS_OBJ(shape->keys[i])) {
        fnc(gc, GAB_VAL_TO_OBJ(shape->keys[i]));
      }
    }
    return;
  }
  case (OBJECT_RECORD): {
    gab_obj_record *object = (gab_obj_record *)obj;
    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.len; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          fnc(gc, GAB_VAL_TO_OBJ(object->dynamic_values.data[i]));
        }
      }
    } else {
      for (u64 i = 0; i < object->static_size; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          fnc(gc, GAB_VAL_TO_OBJ(object->static_values[i]));
        }
      }
    }
    return;
  }
  default:
    return;
  }
}

typedef void (*child_iter)(gab_obj *obj);
static inline void for_child_do(gab_obj *obj, child_iter fnc) {
  gab_obj *child;
  switch (obj->kind) {
  case (OBJECT_CLOSURE): {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->func->nupvalues; i++) {
      child = (gab_obj *)closure->upvalues[i];
      fnc(child);
    }
    return;
  }
  case (OBJECT_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(GAB_VAL_TO_OBJ(*upvalue->data));
    };
    return;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.len; i++) {
      if (GAB_VAL_IS_OBJ(shape->keys[i])) {
        fnc(GAB_VAL_TO_OBJ(shape->keys[i]));
      }
    }
    return;
  }
  case (OBJECT_RECORD): {
    gab_obj_record *object = (gab_obj_record *)obj;

    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.len; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          fnc(GAB_VAL_TO_OBJ(object->dynamic_values.data[i]));
        }
      }
    } else {
      for (u64 i = 0; i < object->static_size; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          fnc(GAB_VAL_TO_OBJ(object->static_values[i]));
        }
      }
    }

    return;
  }
  default:
    return;
  }
}

// Forward declarations - these functions recurse into each other.
static inline void dec_obj_ref(gab_gc *gc, gab_vm *vm, gab_obj *obj);
static inline void dec_if_obj_ref(gab_gc *gc, gab_vm *vm, gab_value val);
static inline void dec_child_refs(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  switch (obj->kind) {
  case OBJECT_CLOSURE: {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->func->nupvalues; i++) {
      dec_if_obj_ref(gc, vm, closure->upvalues[i]);
    }
    return;
  }
  case OBJECT_UPVALUE: {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    dec_if_obj_ref(gc, vm, *upvalue->data);
    return;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.len; i++) {
      dec_if_obj_ref(gc, vm, shape->keys[i]);
    }
    return;
  }
  case OBJECT_RECORD: {
    gab_obj_record *object = (gab_obj_record *)obj;
    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.len; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          dec_if_obj_ref(gc, vm, object->dynamic_values.data[i]);
        }
      }
    } else {
      for (u64 i = 0; i < object->static_size; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          dec_if_obj_ref(gc, vm, object->static_values[i]);
        }
      }
    }
    return;
  }
  default:
    return;
  }
}

static inline void dec_obj_ref(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  if (--obj->references == 0) {
    dec_child_refs(gc, vm, obj);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      queue_destroy(gc, obj);
    }
  } else {
    obj_possible_root(gc, vm, obj);
  }
}

static inline void dec_if_obj_ref(gab_gc *gc, gab_vm *vm, gab_value val) {
  if (GAB_VAL_IS_OBJ(val)) {
    dec_obj_ref(gc, vm, GAB_VAL_TO_OBJ(val));
  }
}

static inline void inc_obj_ref(gab_obj *obj) {
  obj->references++;
  // This object cannot be garbage.
  GAB_OBJ_BLACK(obj);
}

static inline void inc_if_obj_ref(gab_value val) {
  if (GAB_VAL_IS_OBJ(val)) {
    inc_obj_ref(GAB_VAL_TO_OBJ(val));
  }
}

static inline void increment_stack(gab_vm *vm) {
  for (gab_value *tracker = vm->top; tracker > vm->stack; tracker--) {
    inc_if_obj_ref(*tracker);
  }
}

static inline void decrement_stack(gab_gc *gc, gab_vm *vm) {
  gab_value *tracker = vm->top;
  while (--tracker > vm->stack) {
    gab_gc_dref(gc, vm, *tracker);
  }
}

static inline void process_increments(gab_gc *gc) {
  while (gc->increment_count > 0) {
    gc->increment_count--;
    inc_obj_ref(gc->increments[gc->increment_count]);
  }
}

static inline void process_decrements(gab_gc *gc, gab_vm *vm) {
  while (gc->decrement_count > 0) {
    gc->decrement_count--;
    dec_obj_ref(gc, vm, gc->decrements[gc->decrement_count]);
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
    if (d_u64_iexists(&gc->roots, i)) {
      gab_value key = d_u64_ikey(&gc->roots, i);
      gab_obj *obj = GAB_VAL_TO_OBJ(key);

      if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
        mark_gray(obj);
      } else {
        GAB_OBJ_NOT_BUFFERED(obj);

        d_u64_remove(&gc->roots, key);
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
    if (d_u64_iexists(&gc->roots, i)) {
      gab_obj *obj = GAB_VAL_TO_OBJ(d_u64_ikey(&gc->roots, i));
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
    if (d_u64_iexists(&gc->roots, i)) {
      gab_value key = d_u64_ikey(&gc->roots, i);
      gab_obj *obj = GAB_VAL_TO_OBJ(key);

      d_u64_remove(&gc->roots, key);
      GAB_OBJ_NOT_BUFFERED(obj);

      collect_white(gc, obj);
    }
  }
  gc->root_count = 0;
}

void gab_collect_cycles(gab_gc *gc) {
  mark_roots(gc);
  scan_roots(gc);
  collect_roots(gc);
}

void gab_gc_collect(gab_gc *gc, gab_vm *vm) {
  if (vm != NULL)
    increment_stack(vm);

  process_increments(gc);

  process_decrements(gc, vm);

  if (vm != NULL)
    decrement_stack(gc, vm);

  gab_collect_cycles(gc);

  cleanup(gc);
}
