#include "include/gc.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/vm.h"
#include <stdio.h>

#if cGAB_LOG_GC
#define destroy(eg, obj) _destroy(eg, obj, __FILE__, __LINE__)
static inline void _destroy(struct gab_eg *eg, struct gab_obj *obj,
                            const char *file, int line) {
  if (GAB_OBJ_IS_FREED(obj) || GAB_OBJ_IS_NEW(obj)) {
    printf("DFREE\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, file, line);
    exit(1);
  } else {
    GAB_OBJ_FREED(obj);
    printf("FREE\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, file, line);
  }
}
#else
static inline void destroy(struct gab_eg *eg, struct gab_obj *obj) {
  gab_obj_alloc(eg, obj, 0);
}
#endif

void collect_cycles(struct gab_eg *eg, struct gab_gc *gc);

static inline void push_root(struct gab_eg *eg, struct gab_gc *gc,
                             struct gab_obj *obj) {
  if (gc->ndecrements + 1 >= cGAB_GC_ROOT_BUFF_MAX)
    collect_cycles(eg, gc);

  gc->roots[gc->nroots++] = obj;
}

static inline void obj_possible_root(struct gab_eg *eg, struct gab_gc *gc,
                                     struct gab_obj *obj) {
  if (!GAB_OBJ_IS_PURPLE(obj)) {
    GAB_OBJ_PURPLE(obj);
    if (!GAB_OBJ_IS_BUFFERED(obj)) {
#if cGAB_LOG_GC
      printf("ROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

      GAB_OBJ_BUFFERED(obj);
      push_root(eg, gc, obj);
    }
  }
}

static inline void dec_obj_ref(struct gab_eg *eg, struct gab_gc *gc,
                               struct gab_vm *vm, struct gab_obj *obj) {
#if cGAB_LOG_GC
  printf("DEC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references - 1);
#endif
  
  if (--obj->references <= 0) {
    GAB_OBJ_BLACK(obj);

    if (!GAB_OBJ_IS_BUFFERED(obj))
      destroy(eg, obj);

  } else if (!GAB_OBJ_IS_GREEN(obj)) {
    obj_possible_root(eg, gc, obj);
  }
}

static inline void for_child_do(struct gab_obj *obj, gab_gc_visitor fnc,
                                struct gab_gc *gc, struct gab_eg *eg,
                                struct gab_vm *vm) {
  switch (obj->kind) {
  default:
    return;

  case kGAB_BOX: {
    struct gab_obj_box *container = (struct gab_obj_box *)obj;

    if (gab_valiso(container->type))
      fnc(eg, gc, vm, gab_valtoo(container->type));

    if (container->do_visit)
      container->do_visit(eg, gc, vm, fnc, container->data);

    return;
  }

  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = (struct gab_obj_suspense *)obj;

    for (int i = 0; i < sus->len; i++) {
      if (gab_valiso(sus->frame[i]))
        fnc(eg, gc, vm, gab_valtoo(sus->frame[i]));
    }

    return;
  }

  case (kGAB_BLOCK): {
    struct gab_obj_block *b = (struct gab_obj_block *)obj;

    for (int i = 0; i < b->nupvalues; i++) {
      if (gab_valiso(b->upvalues[i]))
        fnc(eg, gc, vm, gab_valtoo(b->upvalues[i]));
    }

    return;
  }

  case (kGAB_MESSAGE): {
    struct gab_obj_message *msg = (struct gab_obj_message *)obj;
    fnc(eg, gc, vm, gab_valtoo(msg->specs));
    return;
  }

  case kGAB_SHAPE: {
    struct gab_obj_shape *shape = (struct gab_obj_shape *)obj;

    for (uint64_t i = 0; i < shape->len; i++) {
      if (gab_valiso(shape->data[i])) {
        fnc(eg, gc, vm, gab_valtoo(shape->data[i]));
      }
    }

    return;
  }

  case kGAB_RECORD: {
    struct gab_obj_record *rec = (struct gab_obj_record *)obj;

    fnc(eg, gc, vm, gab_valtoo(rec->shape));

    for (uint64_t i = 0; i < rec->len; i++) {
      if (gab_valiso(rec->data[i]))
        fnc(eg, gc, vm, gab_valtoo(rec->data[i]));
    }

    return;
  }

    return;
  }
}

void queue_decrement(struct gab_eg *eg, struct gab_gc *gc, struct gab_vm *vm,
                     struct gab_obj *obj) {
  // if (obj->references <= 0)
  //   return;

  if (gc->ndecrements + 1 >= cGAB_GC_DEC_BUFF_MAX)
    gab_gcrun(eg, gc, vm);

#if cGAB_LOG_GC
  printf("QDEC\t%V\t%p\t%i\n", __gab_obj(obj), obj, obj->references);
#endif

  gc->decrements[gc->ndecrements++] = obj;
}

static inline void inc_obj_ref(struct gab_eg *gab, struct gab_gc *gc,
                               struct gab_vm *vm, struct gab_obj *obj) {
  obj->references++;

  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);
  
  if (GAB_OBJ_IS_NEW(obj)) {

#if cGAB_LOG_GC
    printf("NEW\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    gab_obj_old(gab, obj);
  } else {
    for_child_do(obj, queue_decrement, gc, gab, vm);
  }
  
  GAB_OBJ_NOT_NEW(obj);

#if cGAB_LOG_GC
  printf("INC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
}

#if cGAB_DEBUG_GC
bool debug_collect = true;
#endif

#if cGAB_LOG_GC
void __gab_ngciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t stride, size_t len, gab_value values[len],
                   const char *file, int line) {
#else
void gab_ngciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t stride, size_t len, gab_value values[len]) {
#endif
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_gciref(gab, gc, vm, value, file, line);
#else
    gab_gciref(gab, gc, vm, value);
#endif
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
  gab_gcrun(gab, gc, vm);
#endif
}

#if cGAB_LOG_GC
void __gab_ngcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t stride, size_t len, gab_value values[len],
                   const char *file, int line) {
#else
void gab_ngcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t stride, size_t len, gab_value values[len]) {
#endif

#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_gcdref(gab, gc, vm, value, file, line);
#else
    gab_gcdref(gab, gc, vm, value);
#endif
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
  gab_gcrun(gab, gc, vm);
#endif
}

#if cGAB_LOG_GC
gab_value __gab_gciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       gab_value value, const char *file, int32_t line) {
#else
gab_value gab_gciref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     gab_value value) {
#endif
  if (!gab_valiso(value))
    return value;

  struct gab_obj *obj = gab_valtoo(value);

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", value, obj, file, line);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_MODIFIED(obj)) {
    obj->references++;

#if cGAB_LOG_GC
    printf("ELIDE\t%V\t%p\t%d\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           file, line);
#endif

    return value;
  }

#if cGAB_DEBUG_GC
  if (debug_collect)
    gab_gcrun(gab, gc, vm);
#endif

  if (gc->nmodifications + 1 >= cGAB_GC_MOD_BUFF_MAX)
    gab_gcrun(gab, gc, vm);

  GAB_OBJ_MODIFIED(obj);

  inc_obj_ref(gab, gc, vm, obj);

#if cGAB_LOG_GC
  printf("QMOD\t%V\t%p\t%i\t%s:%i\n", __gab_obj(obj), obj, obj->references,
         file, line);
#endif

  gc->modifications[gc->nmodifications++] = obj;
  
  return value;
}

#if cGAB_LOG_GC
gab_value __gab_gcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       gab_value value, const char *file, int32_t line) {
#else
gab_value gab_gcdref(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     gab_value value) {
#endif
  if (!gab_valiso(value))
    return value;

  struct gab_obj *obj = gab_valtoo(value);

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", value, obj, file, line);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_MODIFIED(obj)) {
    obj->references--;

#if cGAB_LOG_GC
    printf("ELIDE\t%V\t%p\t%d\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           file, line);
#endif

    return value;
  }

#if cGAB_DEBUG_GC
  if (debug_collect)
    gab_gcrun(gab, gc, vm);
#endif

  if (gc->nmodifications + 1 >= cGAB_GC_MOD_BUFF_MAX)
    gab_gcrun(gab, gc, vm);

  GAB_OBJ_MODIFIED(obj);

  gc->modifications[gc->nmodifications++] = obj;

#if cGAB_LOG_GC
  printf("QMOD\t%V\t%p\t%i\t%s:%i\n", __gab_obj(obj), obj, obj->references,
         file, line);
#endif

  for_child_do(obj, queue_decrement, gc, gab, vm);

  // queue_decrement(gab, gc, vm, obj);
  // dec_obj_ref(gab, gc, vm, obj);
  obj->references--;

  return value;
}

void gab_gccreate(struct gab_gc *gc) {
  gc->nroots = 0;
  gc->ndecrements = 0;
  gc->nmodifications = 0;
};

void gab_gcdestroy(struct gab_gc *gc) {}

static inline void inc_if_obj_ref(struct gab_eg *eg, struct gab_gc *gc,
                                  struct gab_vm *vm, gab_value val) {
  if (gab_valiso(val))
    inc_obj_ref(eg, gc, vm, gab_valtoo(val));
}

static inline void increment_stack(struct gab_eg *eg, struct gab_gc *gc,
                                   struct gab_vm *vm) {
  gab_value *tracker = vm->sp - 1;

  while (tracker >= vm->sb) {
    inc_if_obj_ref(eg, gc, vm, *tracker--);
  }
}

static inline void decrement_stack(struct gab_eg *gab, struct gab_gc *gc,
                                   struct gab_vm *vm) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  gab_value *tracker = vm->sp - 1;

  while (tracker >= vm->sb) {
    if (gab_valiso(*tracker)) {
      // gab_gcdref(gab, gc, vm, *tracker);
      queue_decrement(gab, gc, vm, gab_valtoo(*tracker--));
    }
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void process_modifications(struct gab_eg *eg, struct gab_gc *gc) {
  while (gc->nmodifications) {
    struct gab_obj *obj = gc->modifications[--gc->nmodifications];

#if cGAB_LOG_GC
    printf("MOD\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    if (obj->references > 0) {
      for_child_do(obj, inc_obj_ref, gc, eg, NULL);

      GAB_OBJ_NOT_MODIFIED(obj);
    } else {
      destroy(eg, obj);
    }
  }
}

static inline void process_decrements(struct gab_eg *eg, struct gab_gc *gc,
                                      struct gab_vm *vm) {
  while (gc->ndecrements)
    dec_obj_ref(eg, gc, vm, gc->decrements[--gc->ndecrements]);
}

static inline void mark_gray(struct gab_obj *obj);

static inline void dec_and_mark_gray(struct gab_eg *eg, struct gab_gc *,
                                     struct gab_vm *vm, struct gab_obj *child) {
  child->references -= 1;
  mark_gray(child);
}

static inline void mark_gray(struct gab_obj *obj) {
  if (!GAB_OBJ_IS_GRAY(obj)) {
    GAB_OBJ_GRAY(obj);
    for_child_do(obj, &dec_and_mark_gray, NULL, NULL, NULL);
  }
}

static inline void mark_roots(struct gab_eg *eg, struct gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    struct gab_obj *obj = gc->roots[i];

    if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
      mark_gray(obj);
    } else {
      GAB_OBJ_NOT_BUFFERED(obj);
      gc->roots[i] = NULL;

      if (GAB_OBJ_IS_BLACK(obj) && obj->references < 1)
        gab_obj_alloc(eg, obj, 0);
    }
  }
}

static inline void scan_root_black(struct gab_obj *obj);
static inline void inc_and_scan_black(struct gab_eg *, struct gab_gc *,
                                      struct gab_vm *, struct gab_obj *child) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(child)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(child), child, __FILE__, __LINE__);
    exit(1);
  }

  printf("INCBLK\t%V\t%p\t%d\n", __gab_obj(child), child, child->references);
#endif

  child->references++;
  if (!GAB_OBJ_IS_BLACK(child))
    scan_root_black(child);
}

static inline void scan_root_black(struct gab_obj *obj) {
  GAB_OBJ_BLACK(obj);
  for_child_do(obj, &inc_and_scan_black, NULL, NULL, NULL);
}

static inline void scan_root(struct gab_eg *, struct gab_gc *, struct gab_vm *,
                             struct gab_obj *obj) {

#if cGAB_LOG_GC
  printf("SCAN\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

  if (GAB_OBJ_IS_GRAY(obj)) {
    if (obj->references > 0) {
      scan_root_black(obj);
    } else {
      GAB_OBJ_WHITE(obj);
      for_child_do(obj, scan_root, NULL, NULL, NULL);
    }
  }
}

static inline void scan_roots(struct gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    struct gab_obj *obj = gc->roots[i];
    if (obj)
      scan_root(NULL, NULL, NULL, obj);
  }
}

static inline void collect_white(struct gab_eg *eg, struct gab_gc *gc,
                                 struct gab_vm *, struct gab_obj *obj) {
  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);

    for_child_do(obj, collect_white, gc, eg, NULL);

    gab_obj_alloc(eg, obj, 0);
  }
}

// Collecting roots is putting me in an infinte loop somehow
static inline void collect_roots(struct gab_eg *eg, struct gab_gc *gc) {
  for (uint64_t i = 0; i < gc->nroots; i++) {
    struct gab_obj *obj = gc->roots[i];
    if (obj) {
      GAB_OBJ_NOT_BUFFERED(obj);
      collect_white(eg, gc, NULL, obj);
      gc->roots[i] = NULL;
    }
  }
}

void collect_cycles(struct gab_eg *eg, struct gab_gc *gc) {
  mark_roots(eg, gc);
  scan_roots(gc);
  collect_roots(eg, gc);
  uint64_t n = gc->nroots;
  gc->nroots = 0;
  for (uint64_t i = 0; i < n; i++) {
    if (gc->roots[i]) {
      gc->roots[gc->nroots++] = gc->roots[i];
    }
  }
}

void gab_gcrun(struct gab_eg *eg, struct gab_gc *gc, struct gab_vm *vm) {

  if (vm != NULL)
    increment_stack(eg, gc, vm);

  process_modifications(eg, gc);

  process_decrements(eg, gc, vm);

  collect_cycles(eg, gc);

  if (vm != NULL)
    decrement_stack(eg, gc, vm);

  gab_mem_reset(eg);
}
