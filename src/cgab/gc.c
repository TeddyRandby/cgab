#include "core.h"
#include "engine.h"
#include "gab.h"
#include <stdio.h>

void queue_decrement(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (gab.gc->decrements.len + 1 >= cGAB_GC_DEC_BUFF_MAX)
    gab_gcrun(gab);

  v_gab_obj_push(&gab.gc->decrements, obj);

#if cGAB_LOG_GC
  printf("QDEC\t%p\t%i\n", obj, obj->references);
#endif
}

void queue_increment(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (gab.gc->increments.len + 1 >= cGAB_GC_DEC_BUFF_MAX)
    gab_gcrun(gab);

  v_gab_obj_push(&gab.gc->increments, obj);

#if cGAB_LOG_GC
  printf("QINC\t%V\t%p\t%i\n", __gab_obj(obj), obj, obj->references);
#endif
}

void queue_root(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_GREEN(obj))
    goto fin;

  if (GAB_OBJ_IS_PURPLE(obj))
    return;

  GAB_OBJ_PURPLE(obj);

fin:
  if (GAB_OBJ_IS_BUFFERED(obj))
    return;

  GAB_OBJ_BUFFERED(obj);

  if (gab.gc->roots.len + 1 >= cGAB_GC_DEC_BUFF_MAX)
    gab_gcrun(gab);

  v_gab_obj_push(&gab.gc->roots, obj);

#if cGAB_LOG_GC
  printf("QROOT\t%V\t%p\t%i\n", __gab_obj(obj), obj, obj->references);
#endif
}

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
  gab_obj_destroy(gab.eg, obj);
  GAB_OBJ_FREED(obj);
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

  obj->references--;

  queue_root(gab, obj);

  if (obj->references == 0) {
    if (!GAB_OBJ_IS_NEW(obj))
      for_child_do(obj, dec_obj_ref, gab);

    GAB_OBJ_BLACK(obj);
  }
}

static inline void inc_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
  printf("INC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references + 1);
#endif

  obj->references++;

  if (!GAB_OBJ_IS_GREEN(obj))
    GAB_OBJ_BLACK(obj);

  if (GAB_OBJ_IS_NEW(obj)) {
#if cGAB_LOG_GC
    printf("NEW\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
    for_child_do(obj, inc_obj_ref, gab);
  }

  GAB_OBJ_NOT_NEW(obj);
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

  queue_increment(gab, obj);

#if cGAB_DEBUG_GC
  gab_gcrun(gab);
#endif

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

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_NEW(obj)) {
    printf("NEWDEC\t%p\n", obj);
  } else {
    printf("DEC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
  }
#endif

#if cGAB_DEBUG_GC
  gab_gcrun(gab);
#endif

  queue_decrement(gab, obj);

  return value;
}

void gab_gccreate(struct gab_gc *gc) {
  v_gab_obj_create(&gc->decrements, cGAB_GC_DEC_BUFF_MAX);
  v_gab_obj_create(&gc->increments, cGAB_GC_MOD_BUFF_MAX);
  v_gab_obj_create(&gc->roots, cGAB_GC_MOD_BUFF_MAX);
  gc->locked = false;
};

void gab_gcdestroy(struct gab_gc *gc) {
  v_gab_obj_destroy(&gc->increments);
  v_gab_obj_destroy(&gc->decrements);
  v_gab_obj_destroy(&gc->roots);
}

static inline void increment_reachable(struct gab_triple gab) {
  size_t stack_size = gab.vm->sp - gab.vm->sb;

  for (size_t i = 0; i < stack_size; i++) {
    if (gab_valiso(gab.vm->sb[i])) {
      struct gab_obj *o = gab_valtoo(gab.vm->sb[i]);
#if cGAB_LOG_GC
      printf("REACHINC\t%V\t%p\n", gab.vm->sb[i], o);
#endif
      inc_obj_ref(gab, o);
    }
  }

  //   for (size_t i = 0; i < gab.eg->scratch.len; i++) {
  //     if (gab_valiso(gab.eg->scratch.data[i])) {
  //       struct gab_obj *o = gab_valtoo(gab.eg->scratch.data[i]);
  // #if cGAB_LOG_GC
  //       printf("REACHINC\t%V\t%p\n", gab.eg->scratch.data[i], o);
  // #endif
  //       inc_obj_ref(gab, o);
  //     }
  //   }
}

static inline void decrement_reachable(struct gab_triple gab) {
  size_t stack_size = gab.vm->sp - gab.vm->sb;

  for (size_t i = 0; i < stack_size; i++) {
    if (gab_valiso(gab.vm->sb[i])) {
      struct gab_obj *o = gab_valtoo(gab.vm->sb[i]);
#if cGAB_LOG_GC
      printf("REACHDEC\t%V\t%p\n", gab.vm->sb[i], o);
#endif
      queue_decrement(gab, o);
    }
  }

  //   for (size_t i = 0; i < gab.eg->scratch.len; i++) {
  //     if (gab_valiso(gab.eg->scratch.data[i])) {
  //       struct gab_obj *o = gab_valtoo(gab.eg->scratch.data[i]);
  // #if cGAB_LOG_GC
  //       printf("REACHDEC\t%V\t%p\n", gab.eg->scratch.data[i], o);
  // #endif
  //
  //       queue_decrement(gab, o);
  //     }
  //   }
}

static inline void process_increments(struct gab_triple gab) {
  while (gab.gc->increments.len) {
    struct gab_obj *obj = v_gab_obj_pop(&gab.gc->increments);

#if cGAB_LOG_GC
    printf("PROCINC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    inc_obj_ref(gab, obj);
  }

  gab.gc->increments.len = 0;
}

static inline void process_decrements(struct gab_triple gab) {
  while (gab.gc->decrements.len) {
    struct gab_obj *o = v_gab_obj_pop(&gab.gc->decrements);
#if cGAB_LOG_GC
    printf("PROCDEC\t%p\n", o);
#endif
    dec_obj_ref(gab, o);
  }

  gab.gc->increments.len = 0;
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
  size_t nroots = gab.gc->roots.len;
  gab.gc->roots.len = 0;

  for (uint64_t i = 0; i < nroots; i++) {
    struct gab_obj *obj = gab.gc->roots.data[i];

#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__,
             __LINE__);
      exit(1);
    }
#endif

    if (GAB_OBJ_IS_PURPLE(obj) && obj->references > 0) {
#if cGAB_LOG_GC
      printf("MARKROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
      v_gab_obj_push(&gab.gc->roots, obj);
      mark_gray(gab, obj);
      continue;
    }

    if (GAB_OBJ_IS_BLACK(obj) && obj->references <= 0) {
#if cGAB_LOG_GC
      printf("DEADROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
      v_gab_obj_push(&gab.gc->roots, obj);
      continue;
    }

    GAB_OBJ_NOT_BUFFERED(obj);

#if cGAB_LOG_GC
    printf("SKIPROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
  }
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
  size_t nroots = gab.gc->roots.len;
  gab.gc->roots.len = 0;

  for (uint64_t i = 0; i < nroots; i++) {
    struct gab_obj *obj = gab.gc->roots.data[i];

#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__,
             __LINE__);
      exit(1);
    }
#endif

    if (GAB_OBJ_IS_BLACK(obj) && obj->references <= 0) {
#if cGAB_LOG_GC
      printf("DEADROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
      destroy(gab, obj);
      continue;
    }

#if cGAB_LOG_GC
    printf("SCANROOT\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    v_gab_obj_push(&gab.gc->roots, obj);
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

  if (GAB_OBJ_IS_WHITE(obj) && !GAB_OBJ_IS_BUFFERED(obj)) {
    GAB_OBJ_BLACK(obj);
    for_child_do(obj, collect_white, gab);
    v_gab_obj_push(&gab.gc->roots, obj);
  }
}

static inline void collect_roots(struct gab_triple gab) {
  const size_t nroots = gab.gc->roots.len;
  gab.gc->roots.len = 0;

  for (uint64_t i = 0; i < nroots; i++) {
    struct gab_obj *obj = gab.gc->roots.data[i];

    if (!GAB_OBJ_IS_BUFFERED(obj)) {
      v_gab_obj_push(&gab.gc->roots, obj);
      continue;
    }

    GAB_OBJ_NOT_BUFFERED(obj);

#if cGAB_LOG_GC
    printf("COLLECTWHITE\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
    collect_white(gab, obj);
  }

  for (uint64_t i = 0; i < gab.gc->roots.len; i++) {
    struct gab_obj *obj = gab.gc->roots.data[i];
    destroy(gab, obj);
  }

  gab.gc->roots.len = 0;
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

  gab_gclock(gab.gc);

  if (gab.vm != NULL)
    increment_reachable(gab);

  process_increments(gab);

  process_decrements(gab);

  collect_cycles(gab);

  if (gab.vm != NULL)
    decrement_reachable(gab);

  gab_gcunlock(gab.gc);
}
