#include "include/gc.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/vm.h"
#include <stdio.h>

#if cGAB_DEBUG_GC
static bool debug_collect = true;
#endif

static inline void for_child_do(struct gab_obj *obj, gab_gc_visitor fnc,
                                struct gab_triple gab) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  switch (obj->kind) {
  default:
    break;

  case kGAB_BOX: {
    struct gab_obj_box *container = (struct gab_obj_box *)obj;

    if (gab_valiso(container->type))
      fnc(gab, gab_valtoo(container->type));

    if (container->do_visit)
      container->do_visit(gab, fnc, container->data);

    break;
  }

  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = (struct gab_obj_suspense *)obj;

    for (int i = 0; i < sus->len; i++) {
      if (gab_valiso(sus->frame[i]))
        fnc(gab, gab_valtoo(sus->frame[i]));
    }

    break;
  }

  case (kGAB_BLOCK): {
    struct gab_obj_block *b = (struct gab_obj_block *)obj;

    for (int i = 0; i < b->nupvalues; i++) {
      if (gab_valiso(b->upvalues[i]))
        fnc(gab, gab_valtoo(b->upvalues[i]));
    }

    break;
  }

  case (kGAB_MESSAGE): {
    struct gab_obj_message *msg = (struct gab_obj_message *)obj;
    fnc(gab, gab_valtoo(msg->specs));
    fnc(gab, gab_valtoo(msg->name));

    break;
  }

  case kGAB_SHAPE: {
    struct gab_obj_shape *shape = (struct gab_obj_shape *)obj;

    for (uint64_t i = 0; i < shape->len; i++) {
      if (gab_valiso(shape->data[i])) {
        fnc(gab, gab_valtoo(shape->data[i]));
      }
    }

    break;
  }

  case kGAB_RECORD: {
    struct gab_obj_record *rec = (struct gab_obj_record *)obj;

    fnc(gab, gab_valtoo(rec->shape));

    for (uint64_t i = 0; i < rec->len; i++) {
      if (gab_valiso(rec->data[i]))
        fnc(gab, gab_valtoo(rec->data[i]));
    }

    break;
  }

#if cGAB_DEBUG_GC
    debug_collect = true;
#endif
  }
}

static inline void dec_obj_ref(struct gab_triple gab, struct gab_obj *obj);

#if cGAB_LOG_GC
#define destroy(gab, obj) _destroy(gab, obj, __FUNCTION__, __LINE__)
static inline void _destroy(struct gab_triple gab, struct gab_obj *obj,
                            const char *file, int line) {
#else
static inline void destroy(struct gab_triple gab, struct gab_obj *obj) {
#endif

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("DFREE\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, file, line);
    exit(1);
  } else {
    printf("FREE\t%V\t%p\t%i\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           file, line);
  }
#else
  gab_obj_destroy(gab.eg, obj);
  gab_memalloc(gab, obj, 0);
#endif
}

void collect_cycles(struct gab_triple gab);

static inline void dec_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
  printf("DEC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references - 1);
#endif

  /*
   * If the object has no references after the decrement,
   * then it can be freed as long as it isn't in the root buffer.
   */
  if (--obj->references <= 0) {
    GAB_OBJ_BLACK(obj);
    
    if (GAB_OBJ_IS_MODIFIED(obj))
      return;

    for_child_do(obj, dec_obj_ref, gab);
    
    destroy(gab, obj);
  }
}

#if cGAB_LOG_GC
static inline void queue_modification(struct gab_triple gab,
                                      struct gab_obj *obj, const char *file,
                                      int line) {
#else
static inline void queue_modification(struct gab_triple gab,
                                      struct gab_obj *obj) {
#endif
  if (gab.gc->nmodifications + 1 >= cGAB_GC_MOD_BUFF_MAX)
    gab_gcrun(gab);

#if cGAB_DEBUG_GC
  if (debug_collect)
    gab_gcrun(gab);
#endif

  gab.gc->modifications[gab.gc->nmodifications++] = obj;

#if cGAB_LOG_GC
  printf("QMOD\t%V\t%p\t%i\t%s:%i\n", __gab_obj(obj), obj, obj->references,
         file, line);
#endif
}

void queue_decrement(struct gab_triple gab, struct gab_obj *obj) {
  if (GAB_OBJ_IS_NEW(obj))
    return;

  if (gab.gc->ndecrements + 1 >= cGAB_GC_DEC_BUFF_MAX)
    gab_gcrun(gab);

#if cGAB_DEBUG_GC
  if (debug_collect)
    gab_gcrun(gab);
#endif

  gab.gc->decrements[gab.gc->ndecrements++] = obj;

#if cGAB_LOG_GC
  printf("QDEC\t%V\t%p\t%i\n", __gab_obj(obj), obj, obj->references);
#endif
}

static inline void inc_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  obj->references++;

  if (GAB_OBJ_IS_NEW(obj)) {
#if cGAB_LOG_GC
    printf("NEW\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
    GAB_OBJ_NOT_NEW(obj);
    return;
  }

  /*
   * If the object is already modified, we leave its color unchanged
   */
  if (GAB_OBJ_IS_MODIFIED(obj)) {

#if cGAB_LOG_GC
    printf("EINC\t%V\t%p\t%d\t%s\n", __gab_obj(obj), obj, obj->references,
           __FUNCTION__);
#endif

    return;
  }

  /*
   * Otherwise, we mark it as black. An incremented object is no
   * longer a candidate for collection or the root of a cycle.
   * If the object was previously purple (a potential root),
   * it no longer needs to be.
   */
  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);

#if cGAB_LOG_GC
  printf("INC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
}

#if cGAB_LOG_GC
void __gab_ngciref(struct gab_triple gab, size_t stride, size_t len,
                   gab_value values[len], const char *func, int line) {
#else
void gab_ngciref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len]) {
#endif
  gab_gcreserve(gab, len);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_gciref(gab, value, func, line);
#else
    gab_gciref(gab, value);
#endif
  }
}

#if cGAB_LOG_GC
void __gab_ngcdref(struct gab_triple gab, size_t stride, size_t len,
                   gab_value values[len], const char *func, int line) {
#else
void gab_ngcdref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len]) {
#endif

  gab_gcreserve(gab, len);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_gcdref(gab, value, func, line);
#else
    gab_gcdref(gab, value);
#endif
  }
}

#if cGAB_LOG_GC
gab_value __gab_gciref(struct gab_triple gab, gab_value value, const char *func,
                       int32_t line) {
#else
gab_value gab_gciref(struct gab_triple gab, gab_value value) {
#endif
  /*
   * If the value is not a heap object, then do nothing
   */
  if (!gab_valiso(value))
    return value;

  struct gab_obj *obj = gab_valtoo(value);

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", value, obj, func, line);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_NEW(obj)) {
    inc_obj_ref(gab, obj);

    if (!GAB_OBJ_IS_MODIFIED(obj)) {
#if cGAB_LOG_GC
      queue_modification(gab, obj, func, line);
#else
      queue_modification(gab, obj);
#endif
      GAB_OBJ_MODIFIED(obj);
    }

    return value;
  }

  if (GAB_OBJ_IS_MODIFIED(obj)) {
    obj->references++;

#if cGAB_LOG_GC
    printf("EINC\t%V\t%p\t%d\t%s\n", __gab_obj(obj), obj, obj->references,
           func);
#endif

    return value;
  }

  /*
   * Pass off to inc_obj_ref
   */
  inc_obj_ref(gab, obj);

#if cGAB_LOG_GC
  queue_modification(gab, obj, func, line);
#else
  queue_modification(gab, obj);
#endif
  GAB_OBJ_MODIFIED(obj);

  /*
   * See gc_dref for an explanation of this.
   */
  for_child_do(obj, queue_decrement, gab);

  return value;
}

#if cGAB_LOG_GC
gab_value __gab_gcdref(struct gab_triple gab, gab_value value, const char *func,
                       int32_t line) {
#else
gab_value gab_gcdref(struct gab_triple gab, gab_value value) {
#endif
  /*
   * If the value is not a heap object, then do nothing
   */
  if (!gab_valiso(value))
    return value;

  struct gab_obj *obj = gab_valtoo(value);

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", value, obj, func, line);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_NEW(obj) && !GAB_OBJ_IS_MODIFIED(obj)) {
    GAB_OBJ_MODIFIED(obj);
#if cGAB_LOG_GC
    queue_modification(gab, obj, func, line);
#else
    queue_modification(gab, obj);
#endif
    return value;
  }

  obj->references--;

  /*
   * If the object doesn't survive, mark it black. This way
   * if it was previously pushed into the root buffer,
   * it will be skipped.
   */
  if (obj->references <= 0)
    GAB_OBJ_BLACK(obj);

  /*
   * Objects are only pushed into the modification buffer
   * at their first modification since the last collection.
   *
   * This is because it is only necessary to know the state of
   * the object at its initial RC, and at its final (during collection).
   */
  if (GAB_OBJ_IS_MODIFIED(obj)) {

#if cGAB_LOG_GC
    printf("EDEC\t%V\t%p\t%d\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           func, line);
#endif

    return value;
  }

  /*
   * This is the objects first modification since the last collection.
   * Queue it into the modification buffer and mark it modified.
   */
#if cGAB_LOG_GC
  queue_modification(gab, obj, func, line);
#else
  queue_modification(gab, obj);
#endif
  GAB_OBJ_MODIFIED(obj);

  /*
   * This is how we *remember* the state of the the object at its
   * initial modification event. Here, we decrement all of its current
   * children.
   *
   * Then, during collection, we increment all the children of each modified
   * object.
   *
   * This way:
   *  - each child that was present at event-time and not at collection-time
   *    receives only this decrement.
   *  - each child that was present at collection-time and not at event-time
   *     receives only the collection-time increment.
   *  - any children present at both event-time and collection-time receive
   *     this decrement *and* the collection-time increment, which cancel out.
   *  - if this object is freed during collection time (while processing
   * modification), the collection time-increment is skipped.
   */
  for_child_do(obj, queue_decrement, gab);

  return value;
}

void gab_gccreate(struct gab_gc *gc) {
  gc->ndecrements = 0;
  gc->nmodifications = 0;
};

void gab_gcdestroy(struct gab_gc *gc) {}

static inline void inc_if_obj_ref(struct gab_triple gab, gab_value val) {
  if (gab_valiso(val))
    inc_obj_ref(gab, gab_valtoo(val));
}

static inline void increment_reachable(struct gab_triple gab) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  gab_value *tracker = gab.vm->sp - 1;

  while (tracker >= gab.vm->sb) {
    inc_if_obj_ref(gab, *tracker);
    tracker--;
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void decrement_reachable(struct gab_triple gab) {
#if cGAB_DEBUG_GC
  debug_collect = false;
#endif

  gab_value *tracker = gab.vm->sp - 1;

  while (tracker >= gab.vm->sb) {
    if (gab_valiso(*tracker)) {
      queue_decrement(gab, gab_valtoo(*tracker));
    }
    tracker--;
  }

#if cGAB_DEBUG_GC
  debug_collect = true;
#endif
}

static inline void process_modifications(struct gab_triple gab) {
  for (size_t i = 0; i < gab.gc->nmodifications; i++) {
    struct gab_obj *obj = gab.gc->modifications[i];

#if cGAB_LOG_GC
    printf("MOD\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    for_child_do(obj, inc_obj_ref, gab);
  }
}

static inline void collect_modifications(struct gab_triple gab) {
  size_t roots = 0;

  for (size_t i = 0; i < gab.gc->nmodifications; i++) {
    struct gab_obj *obj = gab.gc->modifications[i];

    GAB_OBJ_NOT_MODIFIED(obj);

#if cGAB_LOG_GC
    printf("CLEANUP\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    if (obj->references <= 0) {
      for_child_do(obj, dec_obj_ref, gab);

      destroy(gab, obj);

      continue;
    }

    if (GAB_OBJ_IS_GREEN(obj))
      continue;

#if cGAB_LOG_GC
    printf("ROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    GAB_OBJ_MODIFIED(obj);
    GAB_OBJ_WHITE(obj);
    gab.gc->modifications[roots++] = obj;
  }

  gab.gc->nmodifications = roots;
}

static inline void process_decrements(struct gab_triple gab) {
  while (gab.gc->ndecrements)
    dec_obj_ref(gab, gab.gc->decrements[--gab.gc->ndecrements]);
}

static inline void mark_gray(struct gab_triple gab, struct gab_obj *obj);

static inline void dec_and_mark_gray(struct gab_triple gab,
                                     struct gab_obj *child) {
#if cGAB_LOG_GC
  printf("DECGRAY\t%V\t%p\t%d\n", __gab_obj(child), child, child->references);
#endif

  child->references -= 1;
  mark_gray(gab, child);
}

static inline void mark_gray(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (!GAB_OBJ_IS_GRAY(obj)) {
    GAB_OBJ_GRAY(obj);

#if cGAB_LOG_GC
    printf("GRAY\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    for_child_do(obj, &dec_and_mark_gray, gab);
  }
}

static inline void mark_roots(struct gab_triple gab) {
  size_t roots = 0;
  
  for (uint64_t i = 0; i < gab.gc->nmodifications; i++) {
    struct gab_obj *obj = gab.gc->modifications[i];

#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__,
             __LINE__);
      exit(1);
    }
#endif

    if (GAB_OBJ_IS_WHITE(obj)) {
#if cGAB_LOG_GC
      printf("MARKROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
      mark_gray(gab, obj);
      gab.gc->modifications[roots++] = obj;
      continue;
    }
  }

  gab.gc->nmodifications = roots;
}

static inline void scan_root_black(struct gab_triple gab, struct gab_obj *obj);
static inline void inc_and_scan_black(struct gab_triple gab,
                                      struct gab_obj *child) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(child)) {
    printf("UAF\t%V\t%p\t%i\t%s:%i\n", __gab_obj(child), child,
           child->references, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  child->references++;
  if (!GAB_OBJ_IS_BLACK(child))
    scan_root_black(gab, child);

#if cGAB_LOG_GC
  printf("INCBLK\t%V\t%p\t%d\n", __gab_obj(child), child, child->references);
#endif
}

static inline void scan_root_black(struct gab_triple gab, struct gab_obj *obj) {
  GAB_OBJ_BLACK(obj);
  for_child_do(obj, &inc_and_scan_black, gab);
}

static inline void scan_root(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
  printf("SCAN\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

  if (!GAB_OBJ_IS_GRAY(obj))
    return;

  if (obj->references > 0) {
    scan_root_black(gab, obj);
    return;
  }

#if cGAB_LOG_GC
  printf("WHITE\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

  GAB_OBJ_WHITE(obj);
  for_child_do(obj, scan_root, gab);
}

static inline void scan_roots(struct gab_triple gab) {
  for (uint64_t i = 0; i < gab.gc->nmodifications; i++) {
    struct gab_obj *obj = gab.gc->modifications[i];

    if (obj == NULL)
      continue;

#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__,
             __LINE__);
      exit(1);
    }
    printf("SCANROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    scan_root(gab, obj);
  }
}

static inline void collect_white(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__,
             __LINE__);
      exit(1);
    }
#endif

  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_MODIFIED(obj)) {
    GAB_OBJ_BLACK(obj);
    for_child_do(obj, collect_white, gab);
    destroy(gab, obj);
  }
}

static inline void collect_roots(struct gab_triple gab) {
  for (uint64_t i = 0; i < gab.gc->nmodifications; i++) {
    struct gab_obj *obj = gab.gc->modifications[i];

    GAB_OBJ_NOT_MODIFIED(obj);
    
    collect_white(gab, obj);
  }
}

void collect_cycles(struct gab_triple gab) {
  mark_roots(gab);
  scan_roots(gab);
  collect_roots(gab);
}

void gab_gcreserve(struct gab_triple gab, size_t n) {
  if (gab.gc->nmodifications + n >= cGAB_GC_MOD_BUFF_MAX)
    gab_gcrun(gab);
  else if (gab.gc->ndecrements + n >= cGAB_GC_DEC_BUFF_MAX)
    gab_gcrun(gab);
}

void gab_gcrun(struct gab_triple gab) {
  if (gab.vm != NULL)
    increment_reachable(gab);

  process_modifications(gab);

  process_decrements(gab);

  collect_modifications(gab);

  collect_cycles(gab);

  gab.gc->nmodifications = 0;

  if (gab.vm != NULL)
    decrement_reachable(gab);
}
