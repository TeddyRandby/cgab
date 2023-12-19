#include "core.h"
#include "engine.h"
#include "gab.h"
#include <stdio.h>

static inline void for_child_do(struct gab_obj *obj, gab_gc_visitor fnc,
                                struct gab_triple gab) {
  switch (obj->kind) {
  default:
    break;

  case kGAB_BOX: {
    struct gab_obj_box *box = (struct gab_obj_box *)obj;

    if (gab_valiso(box->type))
      fnc(gab, gab_valtoo(box->type));

    if (box->do_visit)
      box->do_visit(gab, fnc, box->len, box->data);

    break;
  }

  case kGAB_SUSPENSE: {
    struct gab_obj_suspense *sus = (struct gab_obj_suspense *)obj;

    for (size_t i = 0; i < sus->nslots; i++) {
      if (gab_valiso(sus->slots[i]))
        fnc(gab, gab_valtoo(sus->slots[i]));
    }

    fnc(gab, gab_valtoo(sus->b));

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
  }
}

static inline void dec_obj_ref(struct gab_triple gab, struct gab_obj *obj);

#if cGAB_LOG_GC
#define destroy(gab, obj) _destroy(gab, obj, __FUNCTION__, __LINE__)
static inline void _destroy(struct gab_triple gab, struct gab_obj *obj,
                            const char *func, int line) {
#else
static inline void destroy(struct gab_triple gab, struct gab_obj *obj) {
#endif
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("DFREE\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, func, line);
    exit(1);
  } else {
    printf("FREE\t%V\t%p\t%i\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           func, line);
  }
  GAB_OBJ_FREED(obj);
  gab_obj_destroy(gab.eg, obj);
#else
  gab_obj_destroy(gab.eg, obj);
  gab_egalloc(gab, obj, 0);
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
  if (--obj->references == 0) {
    GAB_OBJ_BLACK(obj);

    /*
     * This is not really reliable.
     * When an object is freed during GC, we don't know if
     * the children it currently holds references to need to be decremented.
     *
     * When a live object is incremented bc its on the stack,
     * its children are untouched.
     * When that object is freed, its children are dec'd also. But in this case,
     * We never accounted for any changes in the object. BC of this, we may dec
     * objects which were never incremented.
     */
    if (!GAB_OBJ_IS_NEW(obj))
      for_child_do(obj, dec_obj_ref, gab);

    if (GAB_OBJ_IS_MODIFIED(obj))
      return;

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
  if (!gab.gc->running &&
      (gab.gc->modifications.len + 1 >= cGAB_GC_MOD_BUFF_MAX))
    gab_gcrun(gab);

#if cGAB_DEBUG_GC
  else if (!gab.gc->running)
    gab_gcrun(gab);
#endif

  v_gab_obj_push(&gab.gc->modifications, obj);
  GAB_OBJ_MODIFIED(obj);

#if cGAB_LOG_GC
  printf("QMOD\t%V\t%p\t%i\t%s:%i\n", __gab_obj(obj), obj, obj->references,
         file, line);
#endif
}

void queue_decrement(struct gab_triple gab, struct gab_obj *obj) {
  if (!gab.gc->running && (gab.gc->decrements.len + 1 >= cGAB_GC_DEC_BUFF_MAX))
    gab_gcrun(gab);
#if cGAB_DEBUG_GC
  else if (!gab.gc->running)
    gab_gcrun(gab);
#endif

#if cGAB_DEBUG_GC
  else if (!gab.gc->running)
    gab_gcrun(gab);
#endif

  v_gab_obj_push(&gab.gc->decrements, obj);

#if cGAB_LOG_GC
  printf("QDEC\t%p\t%i\n", obj, obj->references);
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

  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);

  if (GAB_OBJ_IS_MODIFIED(obj)) {

#if cGAB_LOG_GC
    printf("EINC\t%V\t%p\t%d\t%s\n", __gab_obj(obj), obj, obj->references,
           __FUNCTION__);
#endif

    return;
  }

  if (GAB_OBJ_IS_NEW(obj)) {
#if cGAB_LOG_GC
    printf("NEW\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

#if cGAB_LOG_GC
    queue_modification(gab, obj, __FUNCTION__, __LINE__);
#else
    queue_modification(gab, obj);
#endif

    GAB_OBJ_NOT_NEW(obj);

    return;
  }

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
  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_gciref(gab, value, func, line);
#else
    gab_gciref(gab, value);
#endif
  }

  gab_gcunlock(gab.gc);
}

#if cGAB_LOG_GC
void __gab_ngcdref(struct gab_triple gab, size_t stride, size_t len,
                   gab_value values[len], const char *func, int line) {
#else
void gab_ngcdref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len]) {
#endif

  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_gcdref(gab, value, func, line);
#else
    gab_gcdref(gab, value);
#endif
  }

  gab_gcunlock(gab.gc);
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

  if (GAB_OBJ_IS_MODIFIED(obj)) {
    obj->references++;

#if cGAB_LOG_GC
    printf("EINC\t%V\t%p\t%d\t%s\n", __gab_obj(obj), obj, obj->references,
           func);
#endif

    return value;
  }

  if (!GAB_OBJ_IS_NEW(obj)) {
    for_child_do(obj, queue_decrement, gab);

#if cGAB_LOG_GC
    queue_modification(gab, obj, func, line);
#else
    queue_modification(gab, obj);
#endif
  }

  inc_obj_ref(gab, obj);

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
    printf("UAF\t%p\t%s:%i\n", obj, func, line);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_NEW(obj)) {
#if cGAB_LOG_GC
    queue_decrement(gab, obj);
#else
    queue_decrement(gab, obj);
#endif
    return value;
  }

  obj->references--;

  if (!GAB_OBJ_IS_GREEN(obj) && obj->references <= 0)
    GAB_OBJ_BLACK(obj);

  if (GAB_OBJ_IS_MODIFIED(obj)) {

#if cGAB_LOG_GC
    printf("EDEC\t%V\t%p\t%d\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           func, line);
#endif

    return value;
  }

  for_child_do(obj, queue_decrement, gab);

#if cGAB_LOG_GC
  queue_modification(gab, obj, func, line);
#else
  queue_modification(gab, obj);
#endif

  return value;
}

void gab_gccreate(struct gab_gc *gc) {
  v_gab_obj_create(&gc->decrements, cGAB_GC_DEC_BUFF_MAX);
  v_gab_obj_create(&gc->modifications, cGAB_GC_MOD_BUFF_MAX);
  gc->running = false;
  gc->locked = false;
};

void gab_gcdestroy(struct gab_gc *gc) {
  v_gab_obj_destroy(&gc->modifications);
  v_gab_obj_destroy(&gc->decrements);
}

static inline void increment_reachable(struct gab_triple gab) {
  size_t stack_size = gab.vm->sp - gab.vm->sb;

  for (size_t i = 0; i <= stack_size; i++) {
    if (gab_valiso(gab.vm->sb[i])) {
      struct gab_obj *o = gab_valtoo(gab.vm->sb[i]);
      printf("REACHINC\t%p\n", o);
      inc_obj_ref(gab, o);
    }
  }
}

static inline void decrement_reachable(struct gab_triple gab) {
  size_t stack_size = gab.vm->sp - gab.vm->sb;

  for (size_t i = 0; i <= stack_size; i++) {
    if (gab_valiso(gab.vm->sb[i])) {
      struct gab_obj *o = gab_valtoo(gab.vm->sb[i]);
      printf("REACHDEC\t%p\n", o);
      // queue_decrement(gab, o);
      gab_gcdref(gab, gab.vm->sb[i]);
    }
  }
}

static inline void process_modifications(struct gab_triple gab) {
  for (size_t i = 0; i < gab.gc->modifications.len; i++) {
    struct gab_obj *obj = v_gab_obj_val_at(&gab.gc->modifications, i);

    for_child_do(obj, inc_obj_ref, gab);

    if (!GAB_OBJ_IS_GREEN(obj))
      GAB_OBJ_WHITE(obj);

#if cGAB_LOG_GC
    printf("MOD\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
  }
}

static inline void collect_modifications(struct gab_triple gab) {
  size_t roots = 0;

  for (size_t i = 0; i < gab.gc->modifications.len; i++) {
    struct gab_obj *obj = v_gab_obj_val_at(&gab.gc->modifications, i);

#if cGAB_LOG_GC
    printf("CLEANUP\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    if (obj->references == 0) {
      // If the object is black, it has already
      // had its children decremented here.
      // But it was not cleaned up, because it was marked as modified.
      if (GAB_OBJ_IS_WHITE(obj))
        for_child_do(obj, dec_obj_ref, gab);

      destroy(gab, obj);

      continue;
    }

    if (GAB_OBJ_IS_GREEN(obj)) {
      GAB_OBJ_NOT_MODIFIED(obj);
      continue;
    }

#if cGAB_LOG_GC
    printf("ROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    v_gab_obj_set(&gab.gc->modifications, roots++, obj);
  }

  gab.gc->modifications.len = roots;
}

static inline void process_decrements(struct gab_triple gab) {
  while (gab.gc->decrements.len) {
    struct gab_obj *o = v_gab_obj_pop(&gab.gc->decrements);
    printf("PROCDEC\t%p\n", o);
    dec_obj_ref(gab, o);
  }
}

static inline void mark_gray(struct gab_triple gab, struct gab_obj *obj);

static inline void dec_and_mark_gray(struct gab_triple gab,
                                     struct gab_obj *child) {
  child->references -= 1;
  mark_gray(gab, child);

#if cGAB_LOG_GC
  printf("DECGRAY\t%V\t%p\t%d\n", __gab_obj(child), child, child->references);
#endif
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

  for (uint64_t i = 0; i < gab.gc->modifications.len; i++) {
    struct gab_obj *obj = v_gab_obj_val_at(&gab.gc->modifications, i);

#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__,
             __LINE__);
      exit(1);
    }
#endif

    if (GAB_OBJ_IS_WHITE(obj) && obj->references > 0) {
#if cGAB_LOG_GC
      printf("MARKROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
      mark_gray(gab, obj);
      v_gab_obj_set(&gab.gc->modifications, roots++, obj);
      continue;
    }

#if cGAB_LOG_GC
    printf("SKIPROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    GAB_OBJ_NOT_MODIFIED(obj);

    if (GAB_OBJ_IS_BLACK(obj) && obj->references <= 0) {
      destroy(gab, obj);
      continue;
    }
  }

  gab.gc->modifications.len = roots;
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
#if cGAB_LOG_GC
    printf("LIVEROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
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
  for (uint64_t i = 0; i < gab.gc->modifications.len; i++) {
    struct gab_obj *obj = v_gab_obj_val_at(&gab.gc->modifications, i);

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
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_MODIFIED(obj)) {
    GAB_OBJ_BLACK(obj);
    for_child_do(obj, collect_white, gab);
    v_gab_obj_push(&gab.gc->modifications, obj);
  }
}

static inline void collect_roots(struct gab_triple gab) {
  const size_t nroots = gab.gc->modifications.len;
  gab.gc->modifications.len = 0;

  for (uint64_t i = 0; i < nroots; i++) {
    struct gab_obj *obj = gab.gc->modifications.data[i];

    GAB_OBJ_NOT_MODIFIED(obj);

#if cGAB_LOG_GC
    printf("COLLECTWHITE\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
    collect_white(gab, obj);
  }

  for (uint64_t i = 0; i < gab.gc->modifications.len; i++) {
    struct gab_obj *obj = v_gab_obj_val_at(&gab.gc->modifications, i);
    destroy(gab, obj);
  }

  gab.gc->modifications.len = 0;
}

void collect_cycles(struct gab_triple gab) {
  mark_roots(gab);
  scan_roots(gab);
  collect_roots(gab);
}

void gab_gclock(struct gab_gc *gc) { gc->locked += 1; }
void gab_gcunlock(struct gab_gc *gc) { gc->locked -= 1; }

void gab_gcrun(struct gab_triple gab) {
  if (gab.gc->locked)
    return;

  gab.gc->running = true;

  if (gab.vm != NULL)
    increment_reachable(gab);

  process_modifications(gab);

  process_decrements(gab);

  collect_cycles(gab);

  collect_modifications(gab);

  if (gab.vm != NULL)
    decrement_reachable(gab);

  gab.gc->running = false;
}
