#include "core.h"
#include "engine.h"
#include "gab.h"
#include <stdint.h>

static inline size_t do_increment(struct gab_gc *gc, struct gab_obj *obj) {
  if (__gab_unlikely(obj->references == INT8_MAX)) {
    size_t rc = d_gab_obj_read(&gc->overflow_rc, obj);

    d_gab_obj_insert(&gc->overflow_rc, obj, rc + 1);

    return rc + 1;
  }

  return obj->references++;
}

static inline size_t do_decrement(struct gab_gc *gc, struct gab_obj *obj) {
  if (__gab_unlikely(obj->references == INT8_MAX)) {
    size_t rc = d_gab_obj_read(&gc->overflow_rc, obj);

    if (__gab_unlikely(rc == UINT8_MAX)) {
      d_gab_obj_remove(&gc->overflow_rc, obj);
      return obj->references--;
    }

    d_gab_obj_insert(&gc->overflow_rc, obj, rc - 1);
    return rc - 1;
  }

  return obj->references--;
}

void queue_decrement(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (gab.gc->decrements.len + 1 >= cGAB_GC_DEC_BUFF_MAX)
    gab_collect(gab);

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
    gab_collect(gab);

  v_gab_obj_push(&gab.gc->increments, obj);

#if cGAB_LOG_GC
  printf("QINC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
}

void queue_destroy(struct gab_triple gab, struct gab_obj* obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_BUFFERED(obj))
    return;

  GAB_OBJ_BUFFERED(obj);

  v_gab_obj_push(&gab.gc->dead, obj);

#if cGAB_LOG_GC
  printf("QDEAD\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
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

  case (kGAB_BLOCK): {
    struct gab_obj_block *b = (struct gab_obj_block *)obj;

    for (int i = 0; i < b->nupvalues; i++) {
      if (gab_valiso(b->upvalues[i]))
        fnc(gab, gab_valtoo(b->upvalues[i]));
    }

    break;
  }

  case kGAB_SHAPE: {
    struct gab_obj_shape *s = (struct gab_obj_shape *)obj;

    for (size_t i = 0; i < s->len; i++) {
      gab_value v = s->keys[i];
      if (gab_valiso(v))
        fnc(gab, gab_valtoo(v));
    }

    for (size_t i = 0; i < s->transitions.len; i++) {
      gab_value v = v_gab_value_val_at(&s->transitions, i);
      if (gab_valiso(v))
        fnc(gab, gab_valtoo(v));
    }

    break;
  }

  case kGAB_RECORD: {
    struct gab_obj_rec *map = (struct gab_obj_rec *)obj;
    size_t len = __builtin_popcountl(map->mask);

    for (size_t i = 0; i < len; i++) {
      size_t offset = i * 2;

      if (gab_valiso(map->data[offset]))
        fnc(gab, gab_valtoo(map->data[offset]));

      if (map->vmask & ((size_t)1 << i)) {
        if (gab_valiso(map->data[offset + 1]))
          fnc(gab, gab_valtoo(map->data[offset + 1]));
      }
    }
    break;
  }

  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *map = (struct gab_obj_recnode *)obj;
    size_t len = __builtin_popcountl(map->mask);

    for (size_t i = 0; i < len; i++) {
      size_t offset = i * 2;

      if (gab_valiso(map->data[offset]))
        fnc(gab, gab_valtoo(map->data[offset]));

      if (map->vmask & ((size_t)1 << i)) {
        if (gab_valiso(map->data[offset + 1]))
          fnc(gab, gab_valtoo(map->data[offset + 1]));
      }
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
    printf("FREE\t%V\t%p\t%i\t%s:%d\n", __gab_obj(obj), obj, obj->references,
           func, line);
  }
  gab_obj_destroy(gab.eg, obj);
  GAB_OBJ_FREED(obj);
#else
  gab_obj_destroy(gab.eg, obj);
  gab_egalloc(gab, obj, 0);
#endif
}

static inline void dec_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
  printf("DEC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references - 1);
#endif

  do_decrement(gab.gc, obj);

  if (obj->references == 0) {
    if (!GAB_OBJ_IS_NEW(obj))
      for_child_do(obj, dec_obj_ref, gab);
  }

  queue_destroy(gab, obj);
}

static inline void inc_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
  printf("INC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references + 1);
#endif

  do_increment(gab.gc, obj);

  if (GAB_OBJ_IS_NEW(obj)) {
#if cGAB_LOG_GC
    printf("NEW\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
    GAB_OBJ_NOT_NEW(obj);
    for_child_do(obj, inc_obj_ref, gab);
  }
}

#if cGAB_LOG_GC
void __gab_niref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len], const char *func, int line) {
#else
void gab_niref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]) {
#endif
  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_iref(gab, value, func, line);
#else
    gab_iref(gab, value);
#endif
  }

  gab_gcunlock(gab.gc);
}

#if cGAB_LOG_GC
void __gab_ndref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len], const char *func, int line) {
#else
void gab_ndref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]) {
#endif

  gab_gclock(gab.gc);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_dref(gab, value, func, line);
#else
    gab_dref(gab, value);
#endif
  }

  gab_gcunlock(gab.gc);
}

#if cGAB_LOG_GC
gab_value __gab_iref(struct gab_triple gab, gab_value value, const char *func,
                     int32_t line) {
#else
gab_value gab_iref(struct gab_triple gab, gab_value value) {
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
  gab_collect(gab);
#endif

  return value;
}

#if cGAB_LOG_GC
gab_value __gab_dref(struct gab_triple gab, gab_value value, const char *func,
                     int32_t line) {
#else
gab_value gab_dref(struct gab_triple gab, gab_value value) {
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
    printf("NEWDEC\t%p\t%d\t%s:%i\n", obj, obj->references, func, line);
  } else {
    printf("DEC\t%V\t%p\t%d\t%s:%i\n", __gab_obj(obj), obj, obj->references,
           func, line);
  }
#endif

#if cGAB_DEBUG_GC
  gab_collect(gab);
#endif

  queue_decrement(gab, obj);

  return value;
}

void gab_gccreate(struct gab_gc *gc) {
  gc->locked = 0;
  d_gab_obj_create(&gc->overflow_rc, 8);
  v_gab_obj_create(&gc->dead, 8);
  v_gab_obj_create(&gc->increments, 8);
  v_gab_obj_create(&gc->decrements, 8);
};

void gab_gcdestroy(struct gab_gc *gc) {
  d_gab_obj_destroy(&gc->overflow_rc);
  v_gab_obj_destroy(&gc->dead);
  v_gab_obj_destroy(&gc->increments);
  v_gab_obj_destroy(&gc->decrements);
}

static inline void increment_reachable(struct gab_triple gab) {
  if (gab.vm != nullptr) {
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
  }

  if (gab_valiso(gab.eg->shapes)) {
    struct gab_obj *o = gab_valtoo(gab.eg->shapes);
    inc_obj_ref(gab, o);
#if cGAB_LOG_GC
    printf("REACHINC\t%V\t%p\n", gab.eg->shapes, o);
#endif
  }

  if (gab_valiso(gab.eg->messages)) {
    struct gab_obj *o = gab_valtoo(gab.eg->messages);
    inc_obj_ref(gab, o);
#if cGAB_LOG_GC
    printf("REACHINC\t%V\t%p\n", gab.eg->messages, o);
#endif
  }
}

static inline void decrement_reachable(struct gab_triple gab) {
  if (gab.vm != nullptr) {
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
  }

  if (gab_valiso(gab.eg->shapes)) {
    struct gab_obj *o = gab_valtoo(gab.eg->shapes);
    queue_decrement(gab, o);
#if cGAB_LOG_GC
    printf("REACHDEC\t%V\t%p\n", gab.eg->shapes, o);
#endif
  }

  if (gab_valiso(gab.eg->messages)) {
    struct gab_obj *o = gab_valtoo(gab.eg->messages);
    queue_decrement(gab, o);
#if cGAB_LOG_GC
    printf("REACHDEC\t%V\t%p\n", gab.eg->messages, o);
#endif
  }
}

static inline void process_increments(struct gab_triple gab) {
  while (gab.gc->increments.len) {
    struct gab_obj *obj = v_gab_obj_pop(&gab.gc->increments);

#if cGAB_LOG_GC
    printf("PROCINC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif

    inc_obj_ref(gab, obj);
  }
}

static inline void process_decrements(struct gab_triple gab) {
  while (gab.gc->decrements.len) {
    struct gab_obj *o = v_gab_obj_pop(&gab.gc->decrements);

#if cGAB_LOG_GC
    printf("PROCDEC\t%V\t%p\t%d\n", __gab_obj(o), o, o->references);
#endif

    dec_obj_ref(gab, o);
  }
}

static inline void collect_dead(struct gab_triple gab) {
  while (gab.gc->dead.len) {
    struct gab_obj* o = v_gab_obj_pop(&gab.gc->dead);

    destroy(gab, o);
  }
}

void gab_gclock(struct gab_gc *gc) {
  assert(gc->locked < UINT8_MAX);
  gc->locked += 1;
}

void gab_gcunlock(struct gab_gc *gc) {
  assert(gc->locked > 0);
  gc->locked -= 1;
}

void gab_collect(struct gab_triple gab) {
  if (gab.gc->locked)
    return;

  gab_gclock(gab.gc);

  increment_reachable(gab);

  process_increments(gab);

  process_decrements(gab);

  decrement_reachable(gab);

  collect_dead(gab);

  gab_gcunlock(gab.gc);
}
