#include "include/engine.h"
#include "include/vm.h"
#include <stdio.h>

#if GAB_DEBUG_GC
boolean debug_collect = true;
#endif

void gab_obj_iref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_obj *obj) {
  if (gc->increment_count + 1 >= INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
  } else {
#if GAB_DEBUG_GC
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  gc->increments[gc->increment_count++] = obj;
}

void gab_obj_dref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_obj *obj) {
  if (gc->decrement_count + 1 >= INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
#if GAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  gc->decrements[gc->decrement_count++] = obj;
}

void gab_gc_iref_many(gab_engine *gab, gab_vm *vm, gab_gc *gc, u64 len,
                      gab_value values[len]) {
  if (gc->increment_count + len >= INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
#if GAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  while (len--) {
    if (GAB_VAL_IS_OBJ(values[len]))
      gc->increments[gc->increment_count++] = GAB_VAL_TO_OBJ(values[len]);
  }
}

void gab_gc_dref_many(gab_engine *gab, gab_vm *vm, gab_gc *gc, u64 len,
                      gab_value values[len]) {
  if (gc->decrement_count + len >= INC_DEC_MAX) {
    gab_gc_collect(gab, vm, gc);
#if GAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  while (len--) {
    if (GAB_VAL_IS_OBJ(values[len]))
      gc->decrements[gc->decrement_count++] = GAB_VAL_TO_OBJ(values[len]);
  }
}

#if GAB_LOG_GC

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

#if GAB_DEBUG_GC
  if (debug_collect)
    gab_gc_collect(gab, vm, gc);
#endif
}

void __gab_gc_dref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_value obj,
                   const char *file, i32 line) {
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

static inline void dump_obj_alloc_stats(gab_gc *gc) {
  fprintf(stdout, "Kind\tObjects\n");
  for (u64 i = 0; i < gc->object_counts.cap; i++) {
    if (d_u64_iexists(&gc->object_counts, i)) {
      fprintf(stdout, "%-8s\t%-6lu\n",
              gab_kind_names[d_u64_ikey(&gc->object_counts, i)],
              d_u64_ival(&gc->object_counts, i));
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
  // Bring the capacity up to be even
  d_gc_set_create(&self->roots, MODULE_CONSTANTS_MAX);
  d_gc_set_create(&self->queue, MODULE_CONSTANTS_MAX);

  self->decrement_count = 0;
  self->increment_count = 0;

#if GAB_LOG_GC
  v_rc_update_create(&self->tracked_decrements, MODULE_CONSTANTS_MAX);
  v_rc_update_create(&self->tracked_increments, MODULE_CONSTANTS_MAX);
  d_rc_tracker_create(&self->tracked_values, MODULE_CONSTANTS_MAX);
  d_u64_create(&self->object_counts, 8);
#endif
};

void gab_gc_destroy(gab_gc *self) {
  d_gc_set_destroy(&self->roots);
  d_gc_set_destroy(&self->queue);

#if GAB_LOG_GC
  fprintf(stdout, "Checking remaining objects...\n");
  for (u64 i = 0; i < self->tracked_values.cap; i++) {
    if (d_rc_tracker_iexists(&self->tracked_values, i)) {
      gab_obj *k = d_rc_tracker_ikey(&self->tracked_values, i);
      i32 v = d_rc_tracker_ival(&self->tracked_values, i);
      if (v > 0) {
        gab_val_dump(GAB_VAL_OBJ(k));
        fprintf(stdout, " had %i remaining references.\n", v);
        dump_rcs_for(self, k);
      }
    }
  }

  fprintf(stdout, "Done.\n");

  dump_obj_alloc_stats(self);

  v_rc_update_destroy(&self->tracked_decrements);
  v_rc_update_destroy(&self->tracked_increments);
  d_rc_tracker_destroy(&self->tracked_values);
  d_u64_destroy(&self->object_counts);
#endif
}

static inline void queue_destroy(gab_engine *gab, gab_vm *vm, gab_gc *self,
                                 gab_obj *obj) {
  d_gc_set_insert(&gab->gc.queue, obj, 0);
}

static inline void cleanup(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  for (u64 i = 0; i < gc->queue.cap; i++) {
    if (d_gc_set_iexists(&gc->queue, i)) {

      gab_obj *obj = d_gc_set_ikey(&gc->queue, i);

      d_gc_set_iremove(&gc->queue, i);
      d_gc_set_remove(&gc->roots, obj);

#if GAB_LOG_GC
      printf("Destroying (#%lu) (%p): ", i, obj);
      gab_val_dump(GAB_VAL_OBJ(obj));
      printf("\n");
      dump_rcs_for(gc, obj);
#endif

      gab_obj_destroy(gab, vm, obj);
      gab_reallocate(gab, obj, gab_obj_size(obj), 0);
    }
  }
}

static inline void push_root(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                             gab_obj *obj) {
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

typedef void (*child_iter)(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                           gab_obj *obj);

static inline void for_child_do(gab_obj *obj, child_iter fnc, gab_engine *gab,
                                gab_vm *vm, gab_gc *gc) {
  switch (obj->kind) {
  case GAB_KIND_STRING:
  case GAB_KIND_PROTOTYPE:
  case GAB_KIND_BUILTIN:
  case GAB_KIND_SYMBOL:
  case GAB_KIND_CONTAINER:
  case GAB_KIND_NIL:
  case GAB_KIND_NUMBER:
  case GAB_KIND_BOOLEAN:
  case GAB_KIND_UNDEFINED:
  case GAB_KIND_NKINDS:
    break;

  case GAB_KIND_EFFECT: {
    gab_obj_effect *eff = (gab_obj_effect *)obj;
    for (u8 i = 0; i < eff->len; i++) {
      if (GAB_VAL_IS_OBJ(eff->frame[i]))
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(eff->frame[i]));
    }
    break;
  }
  case (GAB_KIND_BLOCK): {
    gab_obj_block *closure = (gab_obj_block *)obj;
    for (u8 i = 0; i < closure->nupvalues; i++) {
      if (GAB_VAL_IS_OBJ(closure->upvalues[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(closure->upvalues[i]));
      }
    }
    break;
  }
  case (GAB_KIND_MESSAGE): {
    gab_obj_message *func = (gab_obj_message *)obj;
    for (u64 i = 0; i < func->specs.cap; i++) {
      if (d_specs_iexists(&func->specs, i)) {
        gab_value s = d_specs_ival(&func->specs, i);
        if (GAB_VAL_IS_OBJ(s)) {
          fnc(gab, vm, gc, GAB_VAL_TO_OBJ(s));
        }
      }
    }
    break;
  }
  case (GAB_KIND_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(gab, vm, gc, GAB_VAL_TO_OBJ(*upvalue->data));
    };
    break;
  }
  case GAB_KIND_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->len; i++) {
      if (GAB_VAL_IS_OBJ(shape->data[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(shape->data[i]));
      }
    }
    break;
  }
  case GAB_KIND_LIST: {
    gab_obj_list *lst = (gab_obj_list *)obj;
    for (u64 i = 0; i < lst->data.len; i++) {
      if (GAB_VAL_IS_OBJ(lst->data.data[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(lst->data.data[i]));
      }
    }
    break;
  }
  case GAB_KIND_MAP: {
    gab_obj_map *map = (gab_obj_map *)obj;
    for (u64 i = 0; i < map->data.cap; i++) {
      if (d_gab_value_iexists(&map->data, i)) {
        gab_value k = d_gab_value_ikey(&map->data, i);
        gab_value v = d_gab_value_ival(&map->data, i);
        if (GAB_VAL_IS_OBJ(k))
          fnc(gab, vm, gc, GAB_VAL_TO_OBJ(k));
        if (GAB_VAL_IS_OBJ(v))
          fnc(gab, vm, gc, GAB_VAL_TO_OBJ(v));
      }
    }
    break;
  }
  case GAB_KIND_RECORD: {
    gab_obj_record *rec = (gab_obj_record *)obj;

    for (u64 i = 0; i < rec->len; i++) {
      if (GAB_VAL_IS_OBJ(rec->data[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(rec->data[i]));
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
  case GAB_KIND_STRING:
  case GAB_KIND_PROTOTYPE:
  case GAB_KIND_BUILTIN:
  case GAB_KIND_SYMBOL:
  case GAB_KIND_CONTAINER:
  case GAB_KIND_NIL:
  case GAB_KIND_NUMBER:
  case GAB_KIND_BOOLEAN:
  case GAB_KIND_UNDEFINED:
  case GAB_KIND_NKINDS:
    break;
  case GAB_KIND_BLOCK: {
    gab_obj_block *closure = (gab_obj_block *)obj;
    for (u8 i = 0; i < closure->nupvalues; i++) {
      dec_if_obj_ref(gab, vm, gc, closure->upvalues[i]);
    }
    break;
  }
  case GAB_KIND_EFFECT: {
    gab_obj_effect *eff = (gab_obj_effect *)obj;
    for (u8 i = 0; i < eff->len; i++) {
      dec_if_obj_ref(gab, vm, gc, eff->frame[i]);
    }
    break;
  }
  case (GAB_KIND_MESSAGE): {
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
  case GAB_KIND_UPVALUE: {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    dec_if_obj_ref(gab, vm, gc, *upvalue->data);
    break;
  }
  case GAB_KIND_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->len; i++) {
      dec_if_obj_ref(gab, vm, gc, shape->data[i]);
    }
    break;
  }
  case GAB_KIND_LIST: {
    gab_obj_list *lst = (gab_obj_list *)obj;
    for (u64 i = 0; i < lst->data.len; i++) {
      dec_if_obj_ref(gab, vm, gc, lst->data.data[i]);
    }
    break;
  }
  case GAB_KIND_MAP: {
    gab_obj_map *map = (gab_obj_map *)obj;
    for (u64 i = 0; i < map->data.cap; i++) {
      if (d_gab_value_iexists(&map->data, i)) {
        gab_value k = d_gab_value_ikey(&map->data, i);
        gab_value v = d_gab_value_ival(&map->data, i);
        dec_if_obj_ref(gab, vm, gc, k);
        dec_if_obj_ref(gab, vm, gc, v);
      }
    }
    break;
  }
  case GAB_KIND_RECORD: {
    gab_obj_record *rec = (gab_obj_record *)obj;

    for (u64 i = 0; i < rec->len; i++) {
      dec_if_obj_ref(gab, vm, gc, rec->data[i]);
    }
    break;
  }
  }
}

static inline void dec_obj_ref(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                               gab_obj *obj) {
#if GAB_LOG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references - 1);
#endif
  if (--obj->references <= 0) {
    dec_child_refs(gab, vm, gc, obj);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      queue_destroy(gab, vm, gc, obj);
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
#if GAB_LOG_GC
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
  gab_value *tracker = vm->top - 1;

  while (tracker >= vm->vstack) {

#if GAB_LOG_GC
    if (GAB_VAL_IS_OBJ(*tracker)) {
      v_rc_update_push(&gc->tracked_increments,
                       (rc_update){
                           .val = GAB_VAL_TO_OBJ(*tracker),
                           .file = __FILE__,
                           .line = __LINE__,
                       });
    }
#endif

    inc_if_obj_ref(gc, *tracker--);
  }
}

static inline void decrement_stack(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
#if GAB_DEBUG_GC
  debug_collect = false;
#endif

  gab_value *tracker = vm->top - 1;
  while (tracker >= vm->vstack) {
    gab_gc_dref(gab, vm, gc, *tracker--);
  }

#if GAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void process_increments(gab_gc *gc) {
  while (gc->increment_count) {
    gc->increment_count--;
    inc_obj_ref(gc, gc->increments[gc->increment_count]);
  }
}

static inline void process_decrements(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  while (gc->decrement_count) {
    dec_obj_ref(gab, vm, gc, gc->decrements[--gc->decrement_count]);
  }
}

static inline void mark_gray(gab_obj *obj);

static inline void dec_and_mark_gray(gab_engine *, gab_vm *, gab_gc *,
                                     gab_obj *child) {
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
    for_child_do(obj, &dec_and_mark_gray, NULL, NULL, NULL);
  }
}

static inline void mark_roots(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
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
          queue_destroy(gab, vm, gc, obj);
        }
      }
    }
  }
}

static inline void scan_root_black(gab_obj *obj);
static inline void inc_and_scan_black(gab_engine *, gab_vm *, gab_gc *,
                                      gab_obj *child) {
  child->references++;
  if (!GAB_OBJ_IS_BLACK(child))
    scan_root_black(child);
}

static inline void scan_root_black(gab_obj *obj) {
  GAB_OBJ_BLACK(obj);
  for_child_do(obj, &inc_and_scan_black, NULL, NULL, NULL);
}

static inline void scan_root(gab_engine *, gab_vm *, gab_gc *, gab_obj *obj) {
  if (GAB_OBJ_IS_GRAY(obj)) {
    if (obj->references > 0) {
      scan_root_black(obj);
    } else {
      GAB_OBJ_WHITE(obj);
      for_child_do(obj, &scan_root, NULL, NULL, NULL);
    }
  }
}

static inline void scan_roots(gab_gc *gc) {
  for (i32 i = 0; i < gc->roots.cap; i++) {
    if (d_gc_set_iexists(&gc->roots, i)) {
      gab_obj *obj = d_gc_set_ikey(&gc->roots, i);
      scan_root(NULL, NULL, NULL, obj);
    }
  }
}

static inline void collect_white(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                 gab_obj *obj) {
  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

    for_child_do(obj, collect_white, gab, vm, gc);
    queue_destroy(gab, vm, gc, obj);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  for (u64 i = 0; i < gc->roots.cap; i++) {
    if (d_gc_set_iexists(&gc->roots, i)) {
      gab_obj *obj = d_gc_set_ikey(&gc->roots, i);
      collect_white(gab, vm, gc, obj);
    }
  }
}

void collect_cycles(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  mark_roots(gab, vm, gc);
  scan_roots(gc);
  collect_roots(gab, vm, gc);
}

void gab_gc_collect(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  if (vm != NULL)
    increment_stack(gc, vm);

  process_increments(gc);

  process_decrements(gab, vm, gc);

  collect_cycles(gab, vm, gc);

  if (vm != NULL)
    decrement_stack(gab, vm, gc);

  cleanup(gab, vm, gc);
}
