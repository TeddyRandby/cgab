#include "core.h"
#include "engine.h"
#include "gab.h"

static inline struct gab_obj **get_buf(struct gab_triple gab, uint8_t b,
                                       uint8_t pid, uint8_t epoch) {
  return gab.eg->gc.buffers[b][epoch][pid];
}

static inline size_t get_len(struct gab_triple gab, uint8_t b, uint8_t pid,
                             uint8_t epoch) {
  return gab.eg->gc.buffer_len[b][epoch][pid];
}

static inline void set_len(struct gab_triple gab, uint8_t b, uint8_t pid,
                           uint8_t epoch, size_t len) {
  gab.eg->gc.buffer_len[b][epoch][pid] = len;
}

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
  size_t e = gab.eg->gc.epoch % 2;

  gab_gctrigger(&gab.eg->gc);

  while (get_len(gab, kGAB_BUF_DEC, gab.pid, e) + 1 >= cGAB_GC_DEC_BUFF_MAX)
    thrd_yield();

  auto buf = get_buf(gab, kGAB_BUF_DEC, gab.pid, e);
  auto len = get_len(gab, kGAB_BUF_DEC, gab.pid, e);

  buf[len] = obj;
  set_len(gab, kGAB_BUF_DEC, gab.pid, e, len + 1);

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

  size_t e = gab.eg->gc.epoch % 2;

  gab_gctrigger(&gab.eg->gc);

  while (get_len(gab, kGAB_BUF_INC, gab.pid, e) + 1 >= cGAB_GC_DEC_BUFF_MAX)
    thrd_yield();

  auto buf = get_buf(gab, kGAB_BUF_INC, gab.pid, e);
  auto len = get_len(gab, kGAB_BUF_INC, gab.pid, e);

  buf[len] = obj;
  set_len(gab, kGAB_BUF_INC, gab.pid, e, len + 1);

#if cGAB_LOG_GC
  printf("QINC\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
}

void queue_destroy(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  if (GAB_OBJ_IS_FREED(obj)) {
    printf("UAF\t%V\t%p\t%s:%i\n", __gab_obj(obj), obj, __FUNCTION__, __LINE__);
    exit(1);
  }
#endif

  if (GAB_OBJ_IS_BUFFERED(obj))
    return;

  GAB_OBJ_BUFFERED(obj);

  v_gab_obj_push(&gab.eg->gc.dead, obj);

#if cGAB_LOG_GC
  printf("QDEAD\t%V\t%p\t%d\n", __gab_obj(obj), obj, obj->references);
#endif
}

static inline void for_buf_do(uint8_t b, uint8_t pid, uint8_t epoch,
                              gab_gc_visitor fnc, struct gab_triple gab) {
  auto buf = get_buf(gab, b, pid, epoch);
  auto len = get_len(gab, b, pid, epoch);

  for (size_t i = 0; i < len; i++)
    fnc(gab, buf[i]);
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

  do_decrement(&gab.eg->gc, obj);

  if (obj->references == 0) {
    if (!GAB_OBJ_IS_NEW(obj))
      for_child_do(obj, dec_obj_ref, gab);

    queue_destroy(gab, obj);
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

  do_increment(&gab.eg->gc, obj);

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
  gab_gclock(&gab.eg->gc);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_iref(gab, value, func, line);
#else
    gab_iref(gab, value);
#endif
  }

  gab_gcunlock(&gab.eg->gc);
}

#if cGAB_LOG_GC
void __gab_ndref(struct gab_triple gab, size_t stride, size_t len,
                 gab_value values[len], const char *func, int line) {
#else
void gab_ndref(struct gab_triple gab, size_t stride, size_t len,
               gab_value values[len]) {
#endif

  gab_gclock(&gab.eg->gc);

  for (size_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_dref(gab, value, func, line);
#else
    gab_dref(gab, value);
#endif
  }

  gab_gcunlock(&gab.eg->gc);
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
};

void gab_gcdestroy(struct gab_gc *gc) {
  d_gab_obj_destroy(&gc->overflow_rc);
  v_gab_obj_destroy(&gc->dead);
}

static inline void collect_dead(struct gab_triple gab) {
  while (gab.eg->gc.dead.len) {
    struct gab_obj *o = v_gab_obj_pop(&gab.eg->gc.dead);

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

void processincrements(struct gab_triple gab) {
  size_t epoch = gab.eg->gc.epoch % 2;

  for (uint8_t pid = 0; pid < GAB_NWORKERS; pid++) {
    // For the stack and increment buffers, increment the object
    for_buf_do(kGAB_BUF_STK, pid, epoch, inc_obj_ref, gab);
    for_buf_do(kGAB_BUF_INC, pid, epoch, inc_obj_ref, gab);
    // Reset the length of the inf buffer for this worker
    set_len(gab, kGAB_BUF_INC, pid, epoch, 0);
  }
}

void processdecrements(struct gab_triple gab) {
  size_t epoch = (gab.eg->gc.epoch - 1) % 2;

  for (uint8_t pid = 0; pid < GAB_NWORKERS; pid++) {
    // For the stack and increment buffers, increment the object
    for_buf_do(kGAB_BUF_STK, pid, epoch, dec_obj_ref, gab);
    for_buf_do(kGAB_BUF_DEC, pid, epoch, dec_obj_ref, gab);
    // Reset the length of the inf buffer for this worker
    set_len(gab, kGAB_BUF_DEC, pid, epoch, 0);
  }
}

void processepoch(struct gab_triple gab, size_t pid) {
  /*
   * Copy the stack into this epoch's stack buffer
   */
  struct gab_wk *wk = &gab.eg->workers[pid];
  size_t e = gab.eg->gc.epoch % 2;

  struct gab_obj **buf = get_buf(gab, kGAB_BUF_STK, pid, e);
  struct gab_vm *vm = &wk->vm;

  size_t stack_size = vm->sp - vm->sb;

  assert(stack_size + 1 < cGAB_GC_DEC_BUFF_MAX);

  memcpy(buf, vm->sb, stack_size * sizeof(gab_value));
  buf[stack_size] = gab_valtoo(wk->messages);

  set_len(gab, kGAB_BUF_STK, pid, e, stack_size + 1);

  /*
   * Increment this gc's epoch
   */
  gab.eg->gc.epoch++;
}

void schedule(struct gab_triple gab, size_t pid) {}

void gab_gcepochnext(struct gab_triple gab, size_t pid) {
  if (pid < GAB_NWORKERS) {
    schedule(gab, pid);
  } else {
    gab_collect(gab);
  }
}

void gab_collect(struct gab_triple gab) {
  if (gab.eg->gc.locked)
    return;

  gab_gclock(&gab.eg->gc);

  processincrements(gab);

  processdecrements(gab);

  collect_dead(gab);

  gab_gcunlock(&gab.eg->gc);
}
