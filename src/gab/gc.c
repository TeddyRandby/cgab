#include "include/gc.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/vm.h"
#include <stdio.h>

#if cGAB_DEBUG_GC
bool debug_collect = true;
#endif

#if cGAB_LOG_GC

void __gab_ngciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t stride, size_t len, gab_value values[len],
                   const char *file, int line) {
  if (gc->nincrements + len >= cGAB_GC_INC_BUFF_MAX)
    gab_gcrun(gab, gc, vm);

#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  while (len--)
    __gab_gciref(gab, gc, vm, values[len * stride], file, line);

#if cGAB_DEBUG_GC
  debug_collect = true;
  __gab_gcrun(gab, gc, vm, file, line);
#endif
}

void __gab_ngcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t stride, size_t len, gab_value values[len],
                   const char *file, int line) {
  if (gc->ndecrements + len >= cGAB_GC_DEC_BUFF_MAX) {
    gab_gcrun(gab, gc, vm);
#if cGAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gcrun(gab, gc, vm);
#endif
  }

  while (len--)
    __gab_gcdref(gab, gc, vm, values[len * stride], file, line);
}

gab_value __gab_gciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       gab_value value, const char *file, int32_t line) {
  if (gab_valiso(value)) {
    struct gab_obj *obj = gab_valtoo(value);

#if cGAB_DEBUG_GC || cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", value, obj, file, line);
      exit(1);
    }
#endif

    gc->increments[gc->nincrements++] = obj;

    if (gc->nincrements + 1 >= cGAB_GC_INC_BUFF_MAX) {
      gab_gcrun(gab, gc, vm);
#if cGAB_DEBUG_GC
    } else {
      if (debug_collect)
        gab_gcrun(gab, gc, vm);
#endif
    }

    printf("INC\t%V\t%p\t%s:%i\n", value, obj, file, line);
  }

  return value;
}

gab_value __gab_gcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       gab_value value, const char *file, int32_t line) {
  if (gab_valiso(value)) {
    struct gab_obj *obj = gab_valtoo(value);

#if cGAB_DEBUG_GC || cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", value, obj, file, line);
      exit(1);
    }
#endif

    if (gc->ndecrements + 1 >= cGAB_GC_DEC_BUFF_MAX) {
      gab_gcrun(gab, gc, vm);
#if cGAB_DEBUG_GC
    } else {
      if (debug_collect)
        __gab_gcrun(gab, gc, vm, file, line);
#endif
    }

    gc->decrements[gc->ndecrements++] = obj;
    printf("DEC\t%V\t%p\t%s:%i\n", value, obj, file, line);
  }

  return value;
}

#else

void gab_ngciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t stride, size_t len, gab_value values[len]) {
  if (gc->nincrements + len >= cGAB_GC_INC_BUFF_MAX)
    gab_gcrun(gab, gc, vm);

#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  while (len--)
    gab_gciref(gab, gc, vm, values[len * stride]);

#if cGAB_DEBUG_GC
  debug_collect = true;
  gab_gcrun(gab, gc, vm);
#endif
}

void gab_ngcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t stride, size_t len, gab_value values[len]) {
  if (gc->ndecrements + len >= cGAB_GC_DEC_BUFF_MAX) {
    gab_gcrun(gab, gc, vm);
#if cGAB_DEBUG_GC
  } else {
    if (debug_collect)
      gab_gcrun(gab, gc, vm);
#endif
  }

  while (len--)
    gab_gcdref(gab, gc, vm, values[len * stride]);
}

gab_value gab_gcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     gab_value value) {
  if (gab_valiso(value)) {
    struct gab_obj *obj = gab_valtoo(value);

#if cGAB_DEBUG_GC || cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", value, obj, file, line);
      exit(1);
    }
#endif

    if (gc->ndecrements + 1 >= cGAB_GC_DEC_BUFF_MAX) {
      gab_gcrun(gab, gc, vm);
#if cGAB_DEBUG_GC
    } else {
      if (debug_collect)
        __gab_gcrun(gab, gc, vm, file, line);
#endif
    }

    gc->decrements[gc->ndecrements++] = obj;
  }

  return value;
}

gab_value gab_gciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     gab_value value) {
  if (gab_valiso(value)) {
    struct gab_obj *obj = gab_valtoo(value);

#if cGAB_DEBUG_GC || cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", value, obj, file, line);
      exit(1);
    }
#endif

    gc->increments[gc->nincrements++] = obj;

    if (gc->nincrements + 1 >= cGAB_GC_INC_BUFF_MAX) {
      gab_gcrun(gab, gc, vm);
#if cGAB_DEBUG_GC
    } else {
      if (debug_collect)
        gab_gcrun(gab, gc, vm);
#endif
    }
  }

  return value;
}

#endif

void gab_gccreate(struct gab_gc *gc) {
  gc->nroots = 0;
  gc->ndecrements = 0;
  gc->nincrements = 0;
  memset(&gc->garbage, 0, sizeof(v_gab_obj));
};

void gab_gcdestroy(struct gab_gc *gc) { v_gab_obj_destroy(&gc->garbage); }

static inline void queue_destroy(struct gab_gc *gc, struct gab_obj *obj) {
  if (GAB_OBJ_IS_GARBAGE(obj)) {
    return;
  }

  GAB_OBJ_GARBAGE(obj);
  v_gab_obj_push(&gc->garbage, obj);
}

void collect_cycles(struct gab_gc *gc);

static inline void push_root(struct gab_gc *gc, struct gab_obj *obj) {
  if (gc->ndecrements + 1 >= cGAB_GC_ROOT_BUFF_MAX)
    collect_cycles(gc);

  gc->roots[gc->nroots++] = obj;
}

static inline void obj_possible_root(struct gab_gc *gc, struct gab_obj *obj) {
  if (!GAB_OBJ_IS_PURPLE(obj)) {
    GAB_OBJ_PURPLE(obj);
    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      GAB_OBJ_BUFFERED(obj);
      push_root(gc, obj);
    }
  }
}

static inline void for_child_do(struct gab_obj *obj, gab_gc_visitor fnc,
                                struct gab_gc *gc) {
  switch (obj->kind) {
  default:
    return;

  case kGAB_BOX: {
    struct gab_obj_box *container = (struct gab_obj_box *)obj;

    if (gab_valiso(container->type))
      fnc(gc, gab_valtoo(container->type));

    if (container->do_visit)
      container->do_visit(gc, fnc, container->data);

    return;
  }

  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = (struct gab_obj_suspense *)obj;

    for (int i = 0; i < sus->len; i++) {
      if (gab_valiso(sus->frame[i]))
        fnc(gc, gab_valtoo(sus->frame[i]));
    }

    return;
  }

  case (kGAB_BLOCK): {
    struct gab_obj_block *b = (struct gab_obj_block *)obj;

    for (int i = 0; i < b->nupvalues; i++) {
      if (gab_valiso(b->upvalues[i]))
        fnc(gc, gab_valtoo(b->upvalues[i]));
    }

    return;
  }

  case (kGAB_MESSAGE): {
    struct gab_obj_message *msg = (struct gab_obj_message *)obj;

    for (uint64_t i = 0; i < msg->specs.cap; i++) {
      if (d_specs_iexists(&msg->specs, i)) {
        gab_value r = d_specs_ikey(&msg->specs, i);
        if (gab_valiso(r))
          fnc(gc, gab_valtoo(r));

        gab_value s = d_specs_ival(&msg->specs, i);
        if (gab_valiso(s))
          fnc(gc, gab_valtoo(s));
      }
    }

    return;
  }

  case kGAB_SHAPE: {
    struct gab_obj_shape *shape = (struct gab_obj_shape *)obj;

    for (uint64_t i = 0; i < shape->len; i++) {
      if (gab_valiso(shape->data[i])) {
        fnc(gc, gab_valtoo(shape->data[i]));
      }
    }

    return;
  }

  case kGAB_RECORD: {
    struct gab_obj_record *rec = (struct gab_obj_record *)obj;

    for (uint64_t i = 0; i < rec->len; i++) {
      if (gab_valiso(rec->data[i]))
        fnc(gc, gab_valtoo(rec->data[i]));
    }

    return;
  }

    return;
  }
}

static inline void dec_obj_ref(struct gab_gc *gc, struct gab_obj *obj) {
  if (--obj->references <= 0) {
    for_child_do(obj, dec_obj_ref, gc);

    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj))
      queue_destroy(gc, obj);

  } else if (!GAB_OBJ_IS_GREEN(obj)) {
    obj_possible_root(gc, obj);
  }
}

static inline void inc_obj_ref(struct gab_gc *gc, struct gab_obj *obj) {
  obj->references++;

  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);
}

static inline void inc_if_obj_ref(struct gab_gc *gc, gab_value val) {
  if (gab_valiso(val))
    inc_obj_ref(gc, gab_valtoo(val));
}

static inline void increment_stack(struct gab_gc *gc, struct gab_vm *vm) {
  gab_value *tracker = vm->sp - 1;

  while (tracker >= vm->sb) {

#if cGAB_LOG_GC
    if (gab_valiso(*tracker)) {
      printf("INC\t%V\t%p\t%s:%i\n", *tracker, gab_valtoo(*tracker), __FILE__,
             __LINE__);
    }
#endif

    inc_if_obj_ref(gc, *tracker--);
  }
}

static inline void decrement_stack(struct gab_eg *gab, struct gab_gc *gc,
                                   struct gab_vm *vm) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  gab_value *tracker = vm->sp - 1;
  while (tracker >= vm->sb) {
    gab_gcdref(gab, gc, vm, *tracker--);
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void process_increments(struct gab_gc *gc) {
  while (gc->nincrements)
    inc_obj_ref(gc, gc->increments[--gc->nincrements]);
}

static inline void process_decrements(struct gab_gc *gc) {
  while (gc->ndecrements)
    dec_obj_ref(gc, gc->decrements[--gc->ndecrements]);
}

static inline void mark_gray(struct gab_obj *obj);

static inline void dec_and_mark_gray(struct gab_gc *, struct gab_obj *child) {
  child->references -= 1;
  mark_gray(child);
}

static inline void mark_gray(struct gab_obj *obj) {
  if (!GAB_OBJ_IS_GRAY(obj)) {
    GAB_OBJ_GRAY(obj);
    for_child_do(obj, &dec_and_mark_gray, NULL);
  }
}

static inline void mark_roots(struct gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    struct gab_obj *obj = gc->roots[i];

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

static inline void scan_root_black(struct gab_obj *obj);
static inline void inc_and_scan_black(struct gab_gc *, struct gab_obj *child) {
  child->references++;
  if (!GAB_OBJ_IS_BLACK(child))
    scan_root_black(child);
}

static inline void scan_root_black(struct gab_obj *obj) {
  GAB_OBJ_BLACK(obj);
  for_child_do(obj, &inc_and_scan_black, NULL);
}

static inline void scan_root(struct gab_gc *, struct gab_obj *obj) {
  if (GAB_OBJ_IS_GRAY(obj)) {
    if (obj->references > 0) {
      scan_root_black(obj);
    } else {
      GAB_OBJ_WHITE(obj);
      for_child_do(obj, &scan_root, NULL);
    }
  }
}

static inline void scan_roots(struct gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    struct gab_obj *obj = gc->roots[i];
    if (obj)
      scan_root(NULL, obj);
  }
}

static inline void collect_white(struct gab_gc *gc, struct gab_obj *obj) {
  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

    for_child_do(obj, collect_white, gc);

    queue_destroy(gc, obj);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(struct gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    struct gab_obj *obj = gc->roots[i];
    if (obj) {
      GAB_OBJ_NOT_BUFFERED(obj);
      collect_white(gc, obj);
      gc->roots[i] = NULL;
    }
  }
}

void collect_cycles(struct gab_gc *gc) {
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

#if cGAB_LOG_GC
void __gab_gcrun(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 const char *file, int line) {
  if (vm != NULL)
    increment_stack(gc, vm);

  process_increments(gc);

  process_decrements(gc);

  collect_cycles(gc);

  if (vm != NULL)
    decrement_stack(gab, gc, vm);

  for (size_t i = 0; i < gc->garbage.len; i++) {
    struct gab_obj *obj = v_gab_obj_val_at(&gc->garbage, i);
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("DFREE\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, file, line);
      exit(1);
    } else {
      GAB_OBJ_FREED(obj);
      printf("FREE\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, file, line);
    }
  }

  gc->garbage.len = 0;
}
#else
void gab_gcrun(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  if (vm != NULL)
    increment_stack(gc, vm);

  process_increments(gc);

  process_decrements(gc);

  collect_cycles(gc);

  if (vm != NULL)
    decrement_stack(gab, gc, vm);

  for (size_t i = 0; i < gc->garbage.len; i++)
    gab_obj_destroy(gab, v_gab_obj_val_at(&gc->garbage, i));

  gc->garbage.len = 0;
}
#endif
