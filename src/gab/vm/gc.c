#include "gc.h"
#include "vm.h"

void *gab_gc_reallocate(gab_gc *self, void *loc, u64 old_count, u64 new_count) {
  if (new_count == 0) {
    free(loc);
    return NULL;
  }

#if GAB_DEBUG_GC
  if (new_count > old_count) {
    gab_gc_collect(self);
  };
#endif

  void *new_ptr = realloc(loc, new_count);
  if (new_ptr == NULL) {
    gab_gc_collect(self);
    new_ptr = realloc(loc, new_count);
    if (new_ptr == NULL) {
      exit(1);
    }
  }

  return new_ptr;
}

// Forward declare
void gab_gc_collect_cycles(gab_gc *gc);

static inline void queue_destroy(gab_gc *self, gab_obj *obj) {
  d_u64_insert(&self->queue, GAB_VAL_OBJ(obj), GAB_VAL_NULL(),
               GAB_VAL_OBJ(obj));
}

static inline void trigger_destroy(gab_gc *self) {
  D_U64_FOR_INDEX_WITH_KEY_DO(&self->queue, i) {
    gab_value key = d_u64_index_key(&self->queue, i);
    d_u64_index_remove(&self->queue, i);
    gab_obj_destroy(GAB_VAL_TO_OBJ(key), self->eng);
  }
}

static inline void push_root(gab_gc *self, gab_obj *obj) {
  if (self->root_count >= INC_DEC_MAX) {
    gab_gc_collect(self);
  }

  d_u64_insert(&self->roots, GAB_VAL_OBJ(obj), GAB_VAL_NULL(),
               GAB_VAL_OBJ(obj));

#if GAB_DEBUG_GC
  gab_gc_collect_cycles(self);
#endif
}

static inline void obj_possible_root(gab_gc *self, gab_obj *obj) {
  if (!GAB_OBJ_IS_PURPLE(obj)) {
    GAB_OBJ_PURPLE(obj);
    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      GAB_OBJ_BUFFERED(obj);
      push_root(self, obj);
    }
  }
}

typedef void (*child_and_gc_iter)(gab_gc *gc, gab_obj *obj);
static inline void for_child_and_gc_do(gab_gc *self, gab_obj *obj,
                                       child_and_gc_iter fnc) {
  gab_obj *child;
  switch (obj->kind) {
  case (OBJECT_CLOSURE): {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->func->nupvalues; i++) {
      child = (gab_obj *)closure->upvalues[i];
      fnc(self, child);
    }
    return;
  }
  case (OBJECT_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(self, GAB_VAL_TO_OBJ(*upvalue->data));
    };
    return;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.size; i++) {
      if (GAB_VAL_IS_OBJ(shape->keys[i])) {
        fnc(self, GAB_VAL_TO_OBJ(shape->keys[i]));
      }
    }
    return;
  }
  case (OBJECT_OBJECT): {
    gab_obj_object *object = (gab_obj_object *)obj;
    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.size; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          fnc(self, GAB_VAL_TO_OBJ(object->dynamic_values.data[i]));
        }
      }
    } else {
      for (u64 i = 0; i < object->static_size; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          fnc(self, GAB_VAL_TO_OBJ(object->static_values[i]));
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
    for (u64 i = 0; i < shape->properties.size; i++) {
      if (GAB_VAL_IS_OBJ(shape->keys[i])) {
        fnc(GAB_VAL_TO_OBJ(shape->keys[i]));
      }
    }
    return;
  }
  case (OBJECT_OBJECT): {
    gab_obj_object *object = (gab_obj_object *)obj;

    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.size; i++) {
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
static inline void dec_obj_ref(gab_gc *self, gab_obj *obj);
static inline void dec_if_obj_ref(gab_gc *self, gab_value val);
static inline void dec_child_refs(gab_gc *self, gab_obj *obj) {
  switch (obj->kind) {
  case OBJECT_CLOSURE: {
    gab_obj_closure *closure = (gab_obj_closure *)obj;
    for (u8 i = 0; i < closure->func->nupvalues; i++) {
      gab_obj *upv = (gab_obj *)(closure->upvalues[i]);
      dec_obj_ref(self, upv);
    }
    return;
  }
  case OBJECT_UPVALUE: {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    dec_if_obj_ref(self, *upvalue->data);
    return;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->properties.size; i++) {
      dec_if_obj_ref(self, shape->keys[i]);
    }
    return;
  }
  case OBJECT_OBJECT: {
    gab_obj_object *object = (gab_obj_object *)obj;
    if (object->is_dynamic) {
      for (u64 i = 0; i < object->dynamic_values.size; i++) {
        if (GAB_VAL_IS_OBJ(object->dynamic_values.data[i])) {
          dec_if_obj_ref(self, object->dynamic_values.data[i]);
        }
      }
    } else {
      for (u64 i = 0; i < object->static_size; i++) {
        if (GAB_VAL_IS_OBJ(object->static_values[i])) {
          dec_if_obj_ref(self, object->static_values[i]);
        }
      }
    }
    return;
  }
  default:
    return;
  }
}

static inline void dec_obj_ref(gab_gc *self, gab_obj *obj) {
  if (--obj->references == 0) {
    dec_child_refs(self, obj);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj))
      queue_destroy(self, obj);

  } else {
    obj_possible_root(self, obj);
  }
}

static inline void dec_if_obj_ref(gab_gc *self, gab_value val) {
  if (GAB_VAL_IS_OBJ(val)) {
    gab_obj *obj = GAB_VAL_TO_OBJ(val);
    dec_obj_ref(self, obj);
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

static inline void increment_stack(gab_gc *self) {

#if GAB_LOG_GC
  printf("Incrementing stack...\n");
#endif

  gab_value *tracker = self->eng->vm->stack_top - 1;

  while (tracker >= self->eng->vm->stack) {
    inc_if_obj_ref(*tracker--);
  }
}

static inline void decrement_stack(gab_gc *self) {
#if GAB_LOG_GC
  printf("Decrementing stack...\n");
#endif
  gab_value *tracker = self->eng->vm->stack_top - 1;
  while (tracker >= self->eng->vm->stack) {
    gab_gc_push_dec_if_obj_ref(self, *tracker--);
  }
}

static inline void process_increments(gab_gc *self) {

#if GAB_LOG_GC
  printf("Processing increments for %d objects...\n", self->increment_count);
#endif

  for (u64 i = 0; i < self->increment_count; i++) {
    inc_obj_ref(self->increments[i]);
  }

  self->increment_count = 0;
}

static inline void process_decrements(gab_gc *self) {

#if GAB_LOG_GC
  printf("Processing decrements for %d objects...\n", self->decrement_count);
#endif

  while (self->decrement_count) {
    dec_obj_ref(self, self->decrements[--self->decrement_count]);
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
#if GAB_LOG_GC
    printf("Marking gray: ");
    gab_obj_dump(GAB_VAL_OBJ(obj));
    printf("\n");
#endif
    for_child_do(obj, &dec_and_mark_gray);
  }
}

static inline void mark_roots(gab_gc *self) {
  D_U64_FOR_INDEX_DO(self->roots, i) {
    if (d_u64_index_has_key(&self->roots, i)) {
      gab_value key = d_u64_index_key(&self->roots, i);
      gab_obj *obj = GAB_VAL_TO_OBJ(key);

      if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
        mark_gray(obj);
      } else {
        GAB_OBJ_NOT_BUFFERED(obj);
        // Remove obj from roots somehow
        d_u64_remove(&self->roots, key, key);
        if (GAB_OBJ_IS_BLACK(obj) && obj->references == 0) {
          queue_destroy(self, obj);
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
#if GAB_LOG_GC
  printf("Turned root ");
  gab_obj_dump(GAB_VAL_OBJ(obj));
  printf(" black.\n");
#endif
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
  D_U64_FOR_INDEX_DO(gc->roots, i) {
    if (d_u64_index_has_key(&gc->roots, i)) {
      gab_obj *obj = GAB_VAL_TO_OBJ(d_u64_index_key(&gc->roots, i));
      scan_root(obj);
    }
  }
}

static inline void collect_white(gab_gc *self, gab_obj *obj) {

  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

#if GAB_LOG_GC
    printf("Collected white: ");
    gab_obj_dump(GAB_VAL_OBJ(obj));
    printf(".\n");
#endif

    for_child_and_gc_do(self, obj, collect_white);
    queue_destroy(self, obj);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(gab_gc *gc) {
  D_U64_FOR_INDEX_DO(gc->roots, i) {
    if (d_u64_index_has_key(&gc->roots, i)) {
      gab_value key = d_u64_index_key(&gc->roots, i);
      gab_obj *obj = GAB_VAL_TO_OBJ(key);

      d_u64_remove(&gc->roots, key, key);
      GAB_OBJ_NOT_BUFFERED(obj);

#if GAB_LOG_GC
      printf("Removed ");
      gab_obj_dump(GAB_VAL_OBJ(obj));
      printf(" from the root buffer.\n");
#endif

      collect_white(gc, obj);
    }
  }
  gc->root_count = 0;
}

void gab_gc_collect_cycles(gab_gc *gc) {
#if GAB_LOG_GC
  printf("Beginning cycle collection.\nMarking roots...\n");
#endif
  mark_roots(gc);

  scan_roots(gc);

  collect_roots(gc);
}

void gab_gc_collect(gab_gc *gc) {
  if (gc->active) {
#if GAB_LOG_GC
    printf("Beginning collection...\n");
#endif
    increment_stack(gc);

    process_increments(gc);

    process_decrements(gc);

    decrement_stack(gc);

    trigger_destroy(gc);
#if GAB_LOG_GC
    printf("Collection done.\n");
#endif
  }
}

gab_gc *gab_gc_create(gab_engine *eng) {
  gab_gc *self = CREATE_STRUCT(gab_gc);
  self->root_count = 0;
  self->decrement_count = 0;
  self->increment_count = 0;
  self->active = false;

  d_u64_create(&self->queue, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->roots, MODULE_CONSTANTS_MAX);
  self->eng = eng;

  return self;
}

void gab_gc_destroy(gab_gc *self) {
  d_u64_destroy(&self->queue);
  d_u64_destroy(&self->roots);
  DESTROY_STRUCT(self);
}
