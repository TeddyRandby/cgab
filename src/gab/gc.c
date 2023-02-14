#include "include/gc.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include "include/vm.h"
#include <stdio.h>

#if GAB_DEBUG_GC
boolean debug_collect = true;
#endif

void gab_obj_iref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_obj *obj) {
  if (gc->nincrements + 1 >= GC_INC_BUFF_MAX) {
    gab_gc_collect(gab, vm, gc);
  } else {
#if GAB_DEBUG_GC
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  gc->increments[gc->nincrements++] = obj;
}

void gab_obj_dref(gab_engine *gab, gab_vm *vm, gab_gc *gc, gab_obj *obj) {
  if (gc->ndecrements + 1 >= GC_DEC_BUFF_MAX) {
    gab_gc_collect(gab, vm, gc);
#if GAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  gc->decrements[gc->ndecrements++] = obj;
}

void gab_gc_iref_many(gab_engine *gab, gab_vm *vm, gab_gc *gc, u64 len,
                      gab_value values[len]) {
  if (gc->nincrements + len >= GC_INC_BUFF_MAX) {
    gab_gc_collect(gab, vm, gc);
#if GAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  while (len--) {
    if (GAB_VAL_IS_OBJ(values[len]))
      gc->increments[gc->nincrements++] = GAB_VAL_TO_OBJ(values[len]);
  }
}

void gab_gc_dref_many(gab_engine *gab, gab_vm *vm, gab_gc *gc, u64 len,
                      gab_value values[len]) {
  if (gc->ndecrements + len >= GC_DEC_BUFF_MAX) {
    gab_gc_collect(gab, vm, gc);
#if GAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_collect(gab, vm, gc);
#endif
  }

  while (len--) {
    if (GAB_VAL_IS_OBJ(values[len]))
      gc->decrements[gc->ndecrements++] = GAB_VAL_TO_OBJ(values[len]);
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
  self->ndecrements = 0;
  self->nincrements = 0;
  self->nroots = 0;

#if GAB_LOG_GC
  v_rc_update_create(&self->tracked_decrements, CONSTANTS_MAX);
  v_rc_update_create(&self->tracked_increments, CONSTANTS_MAX);
  d_rc_tracker_create(&self->tracked_values, CONSTANTS_MAX);
  d_u64_create(&self->object_counts, 8);
#endif
};

void gab_gc_destroy(gab_gc *self) {
#if GAB_LOG_GC
  fprintf(stdout, "Checking remaining objects...\n");
  for (u64 i = 0; i < self->tracked_values.cap; i++) {
    if (d_rc_tracker_iexists(&self->tracked_values, i)) {
      gab_obj *k = d_rc_tracker_ikey(&self->tracked_values, i);
      i32 v = d_rc_tracker_ival(&self->tracked_values, i);
      if (v > 0) {
        gab_val_dump(stdout, GAB_VAL_OBJ(k));
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

static inline void queue_destroy(gab_obj *obj) { GAB_OBJ_GARBAGE(obj); }

static inline void cleanup(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  // Maintain the global list of objects
  gab_obj *obj = gab->objects;
  gab_obj *prev = NULL;

  while (obj) {
    if (GAB_OBJ_IS_GARBAGE(obj)) {

#if GAB_LOG_GC
      printf("Destroying (%p): ", obj);
      gab_val_dump(stdout, GAB_VAL_OBJ(obj));
      printf("\n");
      dump_rcs_for(gc, obj);
#endif

      gab_obj *old_obj = obj;
      obj = obj->next;

      gab_obj_destroy(gab, vm, old_obj);
      gab_reallocate(gab, old_obj, gab_obj_size(old_obj), 0);

      if (prev)
        prev->next = obj;

      if (old_obj == gab->objects)
        gab->objects = obj;

    } else {
      prev = obj;
      obj = obj->next;
    }
  }
}

void collect_cycles(gab_engine *gab, gab_vm *vm, gab_gc *gc);
static inline void push_root(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                             gab_obj *obj) {
  if (gc->ndecrements + 1 >= GC_ROOT_BUFF_MAX)
    collect_cycles(gab, vm, gc);

  gc->roots[gc->nroots++] = obj;
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
    return;

  case GAB_KIND_EFFECT: {
    gab_obj_effect *eff = (gab_obj_effect *)obj;
    for (u8 i = 0; i < eff->len; i++) {
      if (GAB_VAL_IS_OBJ(eff->frame[i]))
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(eff->frame[i]));
    }
    return;
  }
  case (GAB_KIND_BLOCK): {
    gab_obj_block *closure = (gab_obj_block *)obj;
    for (u8 i = 0; i < closure->nupvalues; i++) {
      if (GAB_VAL_IS_OBJ(closure->upvalues[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(closure->upvalues[i]));
      }
    }
    return;
  }
  case (GAB_KIND_MESSAGE): {
    gab_obj_message *func = (gab_obj_message *)obj;
    for (u64 i = 0; i < func->specs.cap; i++) {
      if (d_specs_iexists(&func->specs, i)) {
        gab_value r = d_specs_ikey(&func->specs, i);
        if (GAB_VAL_IS_OBJ(r))
          fnc(gab, vm, gc, GAB_VAL_TO_OBJ(r));

        gab_value s = d_specs_ival(&func->specs, i);
        if (GAB_VAL_IS_OBJ(s))
          fnc(gab, vm, gc, GAB_VAL_TO_OBJ(s));
      }
    }
    return;
  }
  case (GAB_KIND_UPVALUE): {
    gab_obj_upvalue *upvalue = (gab_obj_upvalue *)obj;
    if (GAB_VAL_IS_OBJ(*upvalue->data)) {
      fnc(gab, vm, gc, GAB_VAL_TO_OBJ(*upvalue->data));
    };
    return;
  }
  case GAB_KIND_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;
    for (u64 i = 0; i < shape->len; i++) {
      if (GAB_VAL_IS_OBJ(shape->data[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(shape->data[i]));
      }
    }
    return;
  }
  case GAB_KIND_LIST: {
    gab_obj_list *lst = (gab_obj_list *)obj;
    for (u64 i = 0; i < lst->data.len; i++) {
      if (GAB_VAL_IS_OBJ(lst->data.data[i])) {
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(lst->data.data[i]));
      }
    }
    return;
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
    return;
  }
  case GAB_KIND_RECORD: {
    gab_obj_record *rec = (gab_obj_record *)obj;

    for (u64 i = 0; i < rec->len; i++) {
      if (GAB_VAL_IS_OBJ(rec->data[i]))
        fnc(gab, vm, gc, GAB_VAL_TO_OBJ(rec->data[i]));
    }

    return;
  }
  }
}

static inline void dec_obj_ref(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                               gab_obj *obj) {
#if GAB_LOG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references - 1);
#endif
  if (--obj->references <= 0) {
    for_child_do(obj, dec_obj_ref, gab, vm, gc);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj))
      queue_destroy(obj);
  } else if (!GAB_OBJ_IS_GREEN(obj)) {
    obj_possible_root(gab, vm, gc, obj);
  }
}

static inline void inc_obj_ref(gab_gc *gc, gab_obj *obj) {
#if GAB_LOG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references + 1);
#endif
  obj->references++;

  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);
}

static inline void inc_if_obj_ref(gab_gc *gc, gab_value val) {
  if (GAB_VAL_IS_OBJ(val))
    inc_obj_ref(gc, GAB_VAL_TO_OBJ(val));
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
  while (gc->nincrements)
    inc_obj_ref(gc, gc->increments[--gc->nincrements]);
}

static inline void process_decrements(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  while (gc->ndecrements)
    dec_obj_ref(gab, vm, gc, gc->decrements[--gc->ndecrements]);
}

static inline void mark_gray(gab_obj *obj);

static inline void dec_and_mark_gray(gab_engine *, gab_vm *, gab_gc *,
                                     gab_obj *child) {
  child->references -= 1;
  mark_gray(child);
}

static inline void mark_gray(gab_obj *obj) {
  if (!GAB_OBJ_IS_GRAY(obj)) {
    GAB_OBJ_GRAY(obj);
    for_child_do(obj, &dec_and_mark_gray, NULL, NULL, NULL);
  }
}

static inline void mark_roots(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  for (u64 i = 0; i < gc->nroots; i++) {
    gab_obj *obj = gc->roots[i];

    if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
      mark_gray(obj);
    } else {
      GAB_OBJ_NOT_BUFFERED(obj);
      gc->roots[i] = NULL;

      if (GAB_OBJ_IS_BLACK(obj) && obj->references < 1) {
        queue_destroy(obj);
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
  for (u64 i = 0; i < gc->nroots; i++) {
    gab_obj *obj = gc->roots[i];
    if (obj)
      scan_root(NULL, NULL, NULL, obj);
  }
}

static inline void collect_white(gab_engine *gab, gab_vm *vm, gab_gc *gc,
                                 gab_obj *obj) {
  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

    for_child_do(obj, collect_white, gab, vm, gc);

    queue_destroy(obj);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  for (u64 i = 0; i < gc->nroots; i++) {
    gab_obj *obj = gc->roots[i];
    if (obj) {
      GAB_OBJ_NOT_BUFFERED(obj);
      collect_white(gab, vm, gc, obj);
      gc->roots[i] = NULL;
    }
  }
}

void collect_cycles(gab_engine *gab, gab_vm *vm, gab_gc *gc) {
  mark_roots(gab, vm, gc);
  scan_roots(gc);
  collect_roots(gab, vm, gc);
  u64 n = gc->nroots;
  gc->nroots = 0;
  for (u64 i = 0; i < n; i++) {
    if (gc->roots[i]) {
      gc->roots[gc->nroots++] = gc->roots[i];
    }
  }
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
