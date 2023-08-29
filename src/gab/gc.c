#include "include/gc.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/vm.h"
#include <stdio.h>

#if cGAB_DEBUG_GC
bool debug_collect = true;
#endif

void gab_obj_iref(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  gc->increments[gc->nincrements++] = obj;

  if (gc->nincrements + 1 >= cGAB_GC_INC_BUFF_MAX) {
    gab_gc_run(gc, vm);
#if cGAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_run(gc, vm);
#endif
  }
}

void gab_obj_dref(gab_gc *gc, gab_vm *vm, gab_obj *obj) {
  if (gc->ndecrements + 1 >= cGAB_GC_DEC_BUFF_MAX) {
    gab_gc_run(gc, vm);
#if cGAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_run(gc, vm);
#endif
  }

  gc->decrements[gc->ndecrements++] = obj;
}

void gab_ngciref(gab_gc *gc, gab_vm *vm, uint64_t len, gab_value values[len]) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif
  while (len--) {
    if (gab_valiso(values[len]))
      gab_gciref(gc, vm, values[len]);
  }
#if cGAB_DEBUG_GC
  debug_collect = true;
  gab_gc_run(gc, vm);
#endif
}

void gab_ngcdref(gab_gc *gc, gab_vm *vm, uint64_t len, gab_value values[len]) {
  if (gc->ndecrements + len >= cGAB_GC_DEC_BUFF_MAX) {
    gab_gc_run(gc, vm);
#if cGAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gc_run(gc, vm);
#endif
  }

  while (len--) {
    if (gab_valiso(values[len]))
      gc->decrements[gc->ndecrements++] = gab_valtoo(values[len]);
  }
}

#if cGAB_LOG_GC

void __gab_gc_iref(gab_gc *gc, gab_vm *vm, gab_value obj, const char *file,
                   int32_t line) {
  if (GAB_VAL_IS_OBJ(obj)) {
    gab_obj_iref(gc, vm, gab_valtoo(obj));
    v_rc_update_push(&gc->tracked_increments, (rc_update){
                                                  .val = gab_valtoo(obj),
                                                  .file = file,
                                                  .line = line,
                                              });
  }
}

void __gab_gc_dref(gab_gc *gc, gab_vm *vm, gab_value obj, const char *file,
                   int32_t line) {
  if (GAB_VAL_IS_OBJ(obj)) {
    gab_obj_dref(gc, vm, gab_valtoo(obj));
    v_rc_update_push(&gc->tracked_decrements, (rc_update){
                                                  .val = gab_valtoo(obj),
                                                  .file = file,
                                                  .line = line,
                                              });
  }
}

static inline void dump_rcs_for(gab_gc *gc, gab_obj *val) {

  for (uint64_t j = 0; j < gc->tracked_increments.len; j++) {
    rc_update *u = v_rc_update_ref_at(&gc->tracked_increments, j);
    if (u->val == val) {
      fprintf(stdout, "+1 %s:%i.\n", u->file, u->line);
    }
  }

  for (uint64_t j = 0; j < gc->tracked_decrements.len; j++) {
    rc_update *u = v_rc_update_ref_at(&gc->tracked_decrements, j);
    if (u->val == val) {
      fprintf(stdout, "-1 %s:%i.\n", u->file, u->line);
    }
  }
}

#else

void gab_gciref(gab_gc *gc, gab_vm *vm, gab_value obj) {
  if (gab_valiso(obj))
    gab_obj_iref(gc, vm, gab_valtoo(obj));
}

void gab_gcdref(gab_gc *gc, gab_vm *vm, gab_value obj) {
  if (gab_valiso(obj))
    gab_obj_dref(gc, vm, gab_valtoo(obj));
}

#endif

void gab_gc_create(gab_gc *gc) {
  gc->nroots = 0;
  gc->ndecrements = 0;
  gc->nincrements = 0;
};

void gab_gc_destroy(gab_gc *gc) {
#if cGAB_LOG_GC
  fprintf(stdout, "Checking remaining objects...\n");
  for (uint64_t i = 0; i < gc->tracked_values.cap; i++) {
    if (d_rc_tracker_iexists(&gc->tracked_values, i)) {
      gab_obj *k = d_rc_tracker_ikey(&gc->tracked_values, i);
      int32_t v = d_rc_tracker_ival(&gc->tracked_values, i);
      if (v > 0) {
        gab_val_dump(stdout, GAB_VAL_OBJ(k));
        fprintf(stdout, " had %i remaining references.\n", v);
        dump_rcs_for(gc, k);
      }
    }
  }

  fprintf(stdout, "Done.\n");

  v_rc_update_destroy(&gc->tracked_decrements);
  v_rc_update_destroy(&gc->tracked_increments);
  d_rc_tracker_destroy(&gc->tracked_values);
#endif
}

static inline void queue_destroy(gab_gc *gc, gab_obj *obj) {
  v_gab_obj_push(&gc->garbage, obj);
}

void collect_cycles(gab_gc *gc);

static inline void push_root(gab_gc *gc, gab_obj *obj) {
  if (gc->ndecrements + 1 >= cGAB_GC_ROOT_BUFF_MAX)
    collect_cycles(gc);

  gc->roots[gc->nroots++] = obj;
}

static inline void obj_possible_root(gab_gc *gc, gab_obj *obj) {
  if (!GAB_OBJ_IS_PURPLE(obj)) {
    GAB_OBJ_PURPLE(obj);
    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      GAB_OBJ_BUFFERED(obj);
      push_root(gc, obj);
    }
  }
}

static inline void for_child_do(gab_obj *obj, gab_gc_visitor fnc, gab_gc *gc) {
  switch (obj->kind) {
  default:
    return;

  case kGAB_BOX: {
    gab_obj_box *container = (gab_obj_box *)obj;

    if (gab_valiso(container->type))
      fnc(gc, gab_valtoo(container->type));

    if (container->do_visit)
      container->do_visit(gc, fnc, container->data);

    return;
  }

  case kGAB_SUSPENSE: {
    gab_obj_suspense *sus = (gab_obj_suspense *)obj;

    for (uint8_t i = 0; i < sus->len; i++) {
      if (gab_valiso(sus->frame[i]))
        fnc(gc, gab_valtoo(sus->frame[i]));
    }

    fnc(gc, (gab_obj *)sus->p);

    return;
  }

  case (kGAB_BLOCK): {
    gab_obj_block *b = (gab_obj_block *)obj;

    for (uint8_t i = 0; i < b->nupvalues; i++) {
      if (gab_valiso(b->upvalues[i]))
        fnc(gc, gab_valtoo(b->upvalues[i]));
    }

    return;
  }

  case (kGAB_MESSAGE): {
    gab_obj_message *func = (gab_obj_message *)obj;

    for (uint64_t i = 0; i < func->specs.cap; i++) {
      if (d_specs_iexists(&func->specs, i)) {
        gab_value r = d_specs_ikey(&func->specs, i);
        if (gab_valiso(r))
          fnc(gc, gab_valtoo(r));

        gab_value s = d_specs_ival(&func->specs, i);
        if (gab_valiso(s))
          fnc(gc, gab_valtoo(s));
      }
    }

    return;
  }

  case kGAB_SHAPE: {
    gab_obj_shape *shape = (gab_obj_shape *)obj;

    for (uint64_t i = 0; i < shape->len; i++) {
      if (gab_valiso(shape->data[i])) {
        fnc(gc, gab_valtoo(shape->data[i]));
      }
    }

    return;
  }

  case kGAB_RECORD: {
    gab_obj_record *rec = (gab_obj_record *)obj;

    for (uint64_t i = 0; i < rec->len; i++) {
      if (gab_valiso(rec->data[i]))
        fnc(gc, gab_valtoo(rec->data[i]));
    }

    return;
  }

    return;
  }
}

static inline void dec_obj_ref(gab_gc *gc, gab_obj *obj) {
#if cGAB_LOG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references - 1);
#endif
  if (--obj->references <= 0) {
    for_child_do(obj, dec_obj_ref, gc);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj))
      queue_destroy(gc, obj);

  } else if (!GAB_OBJ_IS_GREEN(obj)) {
    obj_possible_root(gc, obj);
  }
}

static inline void inc_obj_ref(gab_gc *gc, gab_obj *obj) {
#if cGAB_LOG_GC
  d_rc_tracker_insert(&gc->tracked_values, obj, obj->references + 1);
#endif
  obj->references++;

  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);
}

static inline void inc_if_obj_ref(gab_gc *gc, gab_value val) {
  if (gab_valiso(val))
    inc_obj_ref(gc, gab_valtoo(val));
}

static inline void increment_stack(gab_gc *gc, gab_vm *vm) {
  gab_value *tracker = vm->sp - 1;

  while (tracker >= vm->sb) {

#if cGAB_LOG_GC
    if (GAB_VAL_IS_OBJ(*tracker)) {
      v_rc_update_push(&gc->tracked_increments, (rc_update){
                                                    .val = gab_valtoo(*tracker),
                                                    .file = __FILE__,
                                                    .line = __LINE__,
                                                });
    }
#endif

    inc_if_obj_ref(gc, *tracker--);
  }
}

static inline void decrement_stack(gab_gc *gc, gab_vm *vm) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  gab_value *tracker = vm->sp - 1;
  while (tracker >= vm->sb) {
    gab_gcdref(gc, vm, *tracker--);
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void process_increments(gab_gc *gc) {
  while (gc->nincrements)
    inc_obj_ref(gc, gc->increments[--gc->nincrements]);
}

static inline void process_decrements(gab_gc *gc) {
  while (gc->ndecrements)
    dec_obj_ref(gc, gc->decrements[--gc->ndecrements]);
}

static inline void mark_gray(gab_obj *obj);

static inline void dec_and_mark_gray(gab_gc *, gab_obj *child) {
  child->references -= 1;
  mark_gray(child);
}

static inline void mark_gray(gab_obj *obj) {
  if (!GAB_OBJ_IS_GRAY(obj)) {
    GAB_OBJ_GRAY(obj);
    for_child_do(obj, &dec_and_mark_gray, NULL);
  }
}

static inline void mark_roots(gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    gab_obj *obj = gc->roots[i];

    if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
      mark_gray(obj);
    } else {
      GAB_OBJ_NOT_BUFFERED(obj);
      gc->roots[i] = NULL;

      if (GAB_OBJ_IS_BLACK(obj) && obj->references < 1)
        queue_destroy(gc, obj);
    }
  }
}

static inline void scan_root_black(gab_obj *obj);
static inline void inc_and_scan_black(gab_gc *, gab_obj *child) {
  child->references++;
  if (!GAB_OBJ_IS_BLACK(child))
    scan_root_black(child);
}

static inline void scan_root_black(gab_obj *obj) {
  GAB_OBJ_BLACK(obj);
  for_child_do(obj, &inc_and_scan_black, NULL);
}

static inline void scan_root(gab_gc *, gab_obj *obj) {
  if (GAB_OBJ_IS_GRAY(obj)) {
    if (obj->references > 0) {
      scan_root_black(obj);
    } else {
      GAB_OBJ_WHITE(obj);
      for_child_do(obj, &scan_root, NULL);
    }
  }
}

static inline void scan_roots(gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    gab_obj *obj = gc->roots[i];
    if (obj)
      scan_root(NULL, obj);
  }
}

static inline void collect_white(gab_gc *gc, gab_obj *obj) {
  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

    for_child_do(obj, collect_white, gc);

    queue_destroy(gc, obj);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    gab_obj *obj = gc->roots[i];
    if (obj) {
      GAB_OBJ_NOT_BUFFERED(obj);
      collect_white(gc, obj);
      gc->roots[i] = NULL;
    }
  }
}

void collect_cycles(gab_gc *gc) {
  mark_roots(gc);
  scan_roots(gc);
  collect_roots(gc);
  uint64_t n = gc->nroots;
  gc->nroots = 0;
  for (uint64_t i = 0; i < n; i++) {
    if (gc->roots[i]) {
      gc->roots[gc->nroots++] = gc->roots[i];
    }
  }
}

void cleanup(gab_gc *gc) {
  while (gc->garbage.len)
    gab_obj_destroy(v_gab_obj_pop(&gc->garbage));
}

void gab_gc_run(gab_gc *gc, gab_vm *vm) {
  v_gab_obj_create(&gc->garbage, 256);

  if (vm != NULL)
    increment_stack(gc, vm);

  process_increments(gc);

  process_decrements(gc);

  collect_cycles(gc);

  if (vm != NULL)
    decrement_stack(gc, vm);

  while (gc->garbage.len)
    gab_obj_destroy(v_gab_obj_pop(&gc->garbage));

  v_gab_obj_destroy(&gc->garbage);
}
