#include "engine.h"
#include "gab.h"
#include <stdatomic.h>
#include <stdint.h>

static inline int32_t epochget(struct gab_triple gab) {
  return gab.eg->jobs[gab.wkid].epoch % GAB_GCNEPOCHS;
}

static inline int32_t epochgetlast(struct gab_triple gab) {
  return (gab.eg->jobs[gab.wkid].epoch - 1) % GAB_GCNEPOCHS;
}

static inline void epochinc(struct gab_triple gab) {
#if cGAB_LOG_GC
  printf("EPOCHINC\t%i\t%i\n", epochget(gab), gab.wkid);
#endif
  gab.eg->jobs[gab.wkid].epoch++;
}

static inline struct gab_obj **bufdata(struct gab_triple gab, uint8_t b,
                                       uint8_t wkid, uint8_t epoch) {
  assert(epoch < GAB_GCNEPOCHS);
  assert(b < kGAB_NBUF);
  assert(wkid < gab.eg->len);
  return gab.eg->gc->buffers[wkid][b][epoch].data;
}

static inline uint64_t buflen(struct gab_triple gab, uint8_t b, uint8_t wkid,
                              uint8_t epoch) {
  assert(epoch < GAB_GCNEPOCHS);
  assert(b < kGAB_NBUF);
  assert(wkid < gab.eg->len);
  return gab.eg->gc->buffers[wkid][b][epoch].len;
}

static inline void bufpush(struct gab_triple gab, uint8_t b, uint8_t wkid,
                           uint8_t epoch, struct gab_obj *o) {
  assert(epoch < GAB_GCNEPOCHS);
  assert(b < kGAB_NBUF);
  assert(wkid < gab.eg->len);
  uint64_t len = buflen(gab, b, wkid, epoch);
  struct gab_obj **buf = bufdata(gab, b, wkid, epoch);
  buf[len] = o;
  gab.eg->gc->buffers[wkid][b][epoch].len = len + 1;
}

static inline void bufclear(struct gab_triple gab, uint8_t b, uint8_t wkid,
                            uint8_t epoch) {
  assert(epoch < GAB_GCNEPOCHS);
  assert(b < kGAB_NBUF);
  assert(wkid < gab.eg->len);
  gab.eg->gc->buffers[wkid][b][epoch].len = 0;
}

static inline uint64_t do_increment(struct gab_gc *gc, struct gab_obj *obj) {
  if (__gab_unlikely(obj->references == INT8_MAX)) {
    uint64_t rc = d_gab_obj_read(&gc->overflow_rc, obj);

    d_gab_obj_insert(&gc->overflow_rc, obj, rc + 1);

    return rc + 1;
  }

  return obj->references++;
}

static inline uint64_t do_decrement(struct gab_gc *gc, struct gab_obj *obj) {
  if (__gab_unlikely(obj->references == INT8_MAX)) {
    uint64_t rc = d_gab_obj_read(&gc->overflow_rc, obj);

    if (__gab_unlikely(rc == UINT8_MAX)) {
      d_gab_obj_remove(&gc->overflow_rc, obj);
      return obj->references--;
    }

    d_gab_obj_insert(&gc->overflow_rc, obj, rc - 1);
    return rc - 1;
  }

  return obj->references--;
}

#if cGAB_LOG_GC
#define queue_decrement(gab, obj)                                              \
  (__queue_decrement(gab, obj, __FUNCTION__, __LINE__))

void __queue_decrement(struct gab_triple gab, struct gab_obj *obj,
                       const char *func, int line) {
#else
void queue_decrement(struct gab_triple gab, struct gab_obj *obj) {
#endif
  int32_t e = epochget(gab);

  gab_gctrigger(gab);

  while (buflen(gab, kGAB_BUF_DEC, gab.wkid, e) >= cGAB_GC_MOD_BUFF_MAX) {
    gab_yield(gab);
    e = epochget(gab);
  }

  bufpush(gab, kGAB_BUF_DEC, gab.wkid, e, obj);

#if cGAB_LOG_GC
  printf("QDEC\t%i\t%p\t%i\t%s:%i\n", epochget(gab), obj, obj->references, func,
         line);
#endif
}

void queue_increment(struct gab_triple gab, struct gab_obj *obj) {
  int32_t e = epochget(gab);

  gab_gctrigger(gab);

  while (buflen(gab, kGAB_BUF_INC, gab.wkid, e) >= cGAB_GC_MOD_BUFF_MAX) {
    if (gab.eg->gc->schedule == gab.wkid)
      gab_gcepochnext(gab);

    e = epochget(gab);

    thrd_yield();
  }

  bufpush(gab, kGAB_BUF_INC, gab.wkid, e, obj);

#if cGAB_LOG_GC
  printf("QINC\t%i\t%p\t%d\n", epochget(gab), obj, obj->references);
#endif
}

void queue_destroy(struct gab_triple gab, struct gab_obj *obj) {
  if (GAB_OBJ_IS_BUFFERED(obj))
    return;

  GAB_OBJ_BUFFERED(obj);

  v_gab_obj_push(&gab.eg->gc->dead, obj);

#if cGAB_LOG_GC
  printf("QDEAD\t%i\t%p\t%d\n", epochget(gab), obj, obj->references);
#endif
}

static inline void for_buf_do(uint8_t b, uint8_t wkid, uint8_t epoch,
                              gab_gc_visitor fnc, struct gab_triple gab) {
  struct gab_obj **buf = bufdata(gab, b, wkid, epoch);
  uint64_t len = buflen(gab, b, wkid, epoch);
#if cGAB_LOG_GC
  printf("FORDO\t%i\t%i\t%lu\n", epoch, wkid, len);
#endif

  for (uint64_t i = 0; i < len; i++) {
    struct gab_obj *obj = buf[i];

#if cGAB_LOG_GC
    if (GAB_OBJ_IS_FREED(obj)) {
      printf("UAF\t%p\n", obj);
      exit(1);
    }
#endif

    fnc(gab, obj);
  }
}

static inline void for_child_do(struct gab_obj *obj, gab_gc_visitor fnc,
                                struct gab_triple gab) {
#if cGAB_LOG_GC
  printf("RECURSE\t%i\t%p\t%i\n", epochget(gab), obj, obj->references);
#endif
  switch (obj->kind) {
  default:
    break;

  case kGAB_FIBER: {
    struct gab_obj_fiber *fib = (struct gab_obj_fiber *)obj;

    for (uint64_t i = 0; i < fib->len; i++) {
      gab_value o = fib->data[i];

      if (gab_valiso(o))
        fnc(gab, gab_valtoo(o));
    }

    break;
  }

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

  case kGAB_SHAPE:
  case kGAB_SHAPELIST: {
    struct gab_obj_shape *s = (struct gab_obj_shape *)obj;

    for (uint64_t i = 0; i < s->len; i++) {
      gab_value v = s->keys[i];
      if (gab_valiso(v))
        fnc(gab, gab_valtoo(v));
    }

    for (uint64_t i = 0; i < s->transitions.len; i++) {
      gab_value v = v_gab_value_val_at(&s->transitions, i);
      if (gab_valiso(v))
        fnc(gab, gab_valtoo(v));
    }

    break;
  }

  case kGAB_RECORD: {
    struct gab_obj_rec *rec = (struct gab_obj_rec *)obj;
    uint64_t len = (rec->len);

    for (uint64_t i = 0; i < len; i++)
      if (gab_valiso(rec->data[i]))
        fnc(gab, gab_valtoo(rec->data[i]));

    break;
  }

  case kGAB_RECORDNODE: {
    struct gab_obj_recnode *rec = (struct gab_obj_recnode *)obj;
    uint64_t len = rec->len;

    for (uint64_t i = 0; i < len; i++)
      if (gab_valiso(rec->data[i]))
        fnc(gab, gab_valtoo(rec->data[i]));

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
    printf("DFREE\t%p\t%s:%i\n", obj, func, line);
    exit(1);
  } else {
    printf("FREE\t%i\t%p\t%i\t%s:%d\n", epochget(gab), obj, obj->references,
           func, line);
  }
  gab_obj_destroy(gab.eg, obj);
  GAB_OBJ_FREED(obj);
#else
  gab_obj_destroy(gab.eg, obj);
  gab_egalloc(gab, obj, 0);
#endif
}

/*
 * ISSUE: When we call process epoch, we move
 * a worker from epoch 0 -> 1. This means it is now
 * queueing decrements into buffer 1.
 *
 * When the GC Process moves to process epoch 0,
 * it processes decrements for the previous epoch -
 * which is also epoch/buffer 1.
 */

static inline void dec_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  printf("DEC\t%i\t%p\t%d\n", epochget(gab), obj, obj->references - 1);
#endif

  do_decrement(gab.eg->gc, obj);

  if (obj->references == 0) {
    if (!GAB_OBJ_IS_NEW(obj))
      for_child_do(obj, dec_obj_ref, gab);

    queue_destroy(gab, obj);
  }
}

static inline void inc_obj_ref(struct gab_triple gab, struct gab_obj *obj) {
#if cGAB_LOG_GC
  printf("INC\t%i\t%p\t%d\n", epochget(gab), obj, obj->references + 1);
#endif

  do_increment(gab.eg->gc, obj);

  if (GAB_OBJ_IS_NEW(obj)) {
#if cGAB_LOG_GC
    printf("NEW\t%i\t%p\n", epochget(gab), obj);
#endif
    GAB_OBJ_NOT_NEW(obj);
    for_child_do(obj, inc_obj_ref, gab);
  }
}

#if cGAB_LOG_GC
void __gab_niref(struct gab_triple gab, uint64_t stride, uint64_t len,
                 gab_value *values, const char *func, int line) {
#else
void gab_niref(struct gab_triple gab, uint64_t stride, uint64_t len,
               gab_value *values) {
#endif
  for (uint64_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_iref(gab, value, func, line);
#else
    gab_iref(gab, value);
#endif
  }
}

#if cGAB_LOG_GC
void __gab_ndref(struct gab_triple gab, uint64_t stride, uint64_t len,
                 gab_value *values, const char *func, int line) {
#else
void gab_ndref(struct gab_triple gab, uint64_t stride, uint64_t len,
               gab_value *values) {
#endif

  for (uint64_t i = 0; i < len; i++) {
    gab_value value = values[i * stride];

#if cGAB_LOG_GC
    __gab_dref(gab, value, func, line);
#else
    gab_dref(gab, value);
#endif
  }
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
  printf("IREF\t%i\t%p\t%d\t%s:%i\n", epochget(gab), obj, obj->references, func,
         line);
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

#if cGAB_DEBUG_GC
  gab_collect(gab);
#endif

#if cGAB_LOG_GC
  if (GAB_OBJ_IS_NEW(obj)) {
    printf("NEWDREF\t%i\t%p\t%d\t%s:%i\n", epochget(gab), obj, obj->references,
           func, line);
  } else {
    printf("DREF\t%i\t%p\t%d\t%s:%i\n", epochget(gab), obj, obj->references,
           func, line);
  }
#endif

#if cGAB_LOG_GC
  __queue_decrement(gab, obj, func, line);
#else
  queue_decrement(gab, obj);
#endif

  return value;
}

void gab_gccreate(struct gab_triple gab) {
  gab.eg->gc->schedule = -1;
  d_gab_obj_create(&gab.eg->gc->overflow_rc, 8);
  v_gab_obj_create(&gab.eg->gc->dead, 8);

  for (int i = 0; i < gab.eg->len; i++) {
    for (int b = 0; b < kGAB_NBUF; b++) {
      for (int e = 0; e < GAB_GCNEPOCHS; e++) {
        bufclear(gab, b, i, e);
      }
    }
  }
};

void gab_gcdestroy(struct gab_triple gab) {
  d_gab_obj_destroy(&gab.eg->gc->overflow_rc);
  v_gab_obj_destroy(&gab.eg->gc->dead);
}

static inline void collect_dead(struct gab_triple gab) {
  while (gab.eg->gc->dead.len) {
    struct gab_obj *o = v_gab_obj_pop(&gab.eg->gc->dead);

    destroy(gab, o);
  }
}

void gab_gclock(struct gab_triple gab) {
  struct gab_jb *wk = gab.eg->jobs + gab.wkid;
  assert(wk->locked < UINT8_MAX);
  wk->locked += 1;
}

/*
 * There was a bug where objects were beging freed *while they were in the locke
 * queue*. This was resolved by marking locked objects as *buffered* until they
 * are unlocked.
 */
void gab_gcunlock(struct gab_triple gab) {
  struct gab_jb *wk = gab.eg->jobs + gab.wkid;
  assert(wk->locked > 0);
  wk->locked -= 1;

  if (!wk->locked) {
    gab_ndref(gab, 1, wk->lock_keep.len, wk->lock_keep.data);

    for (uint64_t i = 0; i < wk->lock_keep.len; i++)
      GAB_OBJ_NOT_BUFFERED(gab_valtoo(v_gab_value_val_at(&wk->lock_keep, i)));

    wk->lock_keep.len = 0;
  }
}

void processincrements(struct gab_triple gab, int32_t epoch) {
#if cGAB_LOG_GC
  printf("IEPOCH\t%i\n", epoch);
#endif

  for (uint8_t wkid = 0; wkid < gab.eg->len; wkid++) {
    // For the stack and increment buffers, increment the object
    for_buf_do(kGAB_BUF_STK, wkid, epoch, inc_obj_ref, gab);
    for_buf_do(kGAB_BUF_INC, wkid, epoch, inc_obj_ref, gab);
    // Reset the length of the inf buffer for this worker
    bufclear(gab, kGAB_BUF_STK, wkid, epoch);
    bufclear(gab, kGAB_BUF_INC, wkid, epoch);
  }
#if cGAB_LOG_GC
  printf("IEPOCH!\t%i\n", epoch);
#endif
}

void processdecrements(struct gab_triple gab, int32_t epoch) {
#if cGAB_LOG_GC
  printf("DEPOCH\t%i\n", epoch);
#endif

  for (uint8_t wkid = 0; wkid < gab.eg->len; wkid++) {
    // For the stack and increment buffers, increment the object
    for_buf_do(kGAB_BUF_STK, wkid, epoch, dec_obj_ref, gab);
    for_buf_do(kGAB_BUF_DEC, wkid, epoch, dec_obj_ref, gab);
    // Reset the length of the dec buffer for this worker
    bufclear(gab, kGAB_BUF_STK, wkid, epoch);
    bufclear(gab, kGAB_BUF_DEC, wkid, epoch);
  }

#if cGAB_LOG_GC
  printf("DEPOCH!\t%i\n", epoch);
#endif
}

void processepoch(struct gab_triple gab, int32_t e) {
  struct gab_jb *wk = &gab.eg->jobs[gab.wkid];

#if cGAB_LOG_GC
  printf("PEPOCH\t%i\t%i\n", e, gab.wkid);
#endif

  if (wk->fiber == gab_undefined) {
    goto fin;
  }

  assert(gab_valkind(wk->fiber) == kGAB_FIBER ||
         gab_valkind(wk->fiber) == kGAB_FIBERRUNNING ||
         gab_valkind(wk->fiber) == kGAB_FIBERDONE);
  struct gab_obj_fiber *fb = GAB_VAL_TO_FIBER(wk->fiber);

  struct gab_vm *vm = &fb->vm;

  uint64_t stack_size = vm->sp - vm->sb;

  assert(stack_size + wk->lock_keep.len + 2 < cGAB_GC_MOD_BUFF_MAX);

  bufpush(gab, kGAB_BUF_STK, gab.wkid, e, gab_valtoo(wk->fiber));
  bufpush(gab, kGAB_BUF_STK, gab.wkid, e, gab_valtoo(fb->messages));

  for (uint64_t i = 0; i < stack_size; i++) {
    if (gab_valiso(vm->sb[i])) {
      struct gab_obj *o = gab_valtoo(vm->sb[i]);
#if cGAB_LOG_GC
      printf("SAVESTK\t%i\t%p\t%d\n", epochget(gab), (void *)o, o->kind);
#endif
      bufpush(gab, kGAB_BUF_STK, gab.wkid, e, o);
    }
  }

fin:
  epochinc(gab);
#if cGAB_LOG_GC
  printf("PEPOCH!\t%i\t%i\n", epochget(gab), gab.wkid);
#endif
}

void assert_workers_have_epoch(struct gab_triple gab, int32_t e) {
  printf("EXPECTING EPOCH %i BASED ON %i\n", e, gab.wkid);

  for (uint64_t i = 1; i < gab.eg->len; i++) {
    int32_t this_e = epochget((struct gab_triple){gab.eg, .wkid = i});
    printf("WORKER %li IN EPOCH %i (%i)\n", i, this_e, gab.eg->jobs[i].epoch);
    assert(this_e == e);
  }
}

void schedule(struct gab_triple gab, uint64_t wkid) {
#if cGAB_LOG_GC
  if (wkid < gab.eg->len)
    printf("SCHEDULE EPOCH %i\t%lu\n", gab.eg->jobs[wkid].epoch, wkid);
#endif

  // Wrap around the number of jobs. Since
  // The 0th job is the GC job, we will wrap around
  // and begin the gc last.
  if (wkid >= gab.eg->len) {
#if cGAB_LOG_GC
    int32_t expected_e = (gab.eg->jobs[0].epoch + 1) % 3;
    assert_workers_have_epoch(gab, expected_e);
#endif

    gab.eg->gc->schedule = 0;
    return;
  }

  // If the worker we're scheduling for isn't alive, skip it
  if (!gab.eg->jobs[wkid].alive) {
    gab.eg->jobs[wkid].epoch++;
#if cGAB_LOG_GC
    printf("SKIP EPOCH %i\t%lu\n", gab.eg->jobs[wkid].epoch, wkid);
#endif

    assert(!gab.eg->jobs[wkid].alive);
    schedule(gab, wkid + 1);
    return;
  }

  if (gab.eg->gc->schedule < (int8_t)wkid)
    gab.eg->gc->schedule = wkid;
}

#if cGAB_LOG_GC
void __gab_gcepochnext(struct gab_triple gab, const char *func, int line) {

  printf("EPOCH\t%i\t%i\t%s:%i\n", epochget(gab), gab.wkid, func, line);
#else
void gab_gcepochnext(struct gab_triple gab) {
#endif
  if (gab.wkid > 0)
    processepoch(gab, epochget(gab));

  schedule(gab, gab.wkid + 1);
}

bool gab_gctrigger(struct gab_triple gab) {
  if (gab.eg->gc->schedule >= 0)
    return false;

  uint64_t e = epochget(gab);

  for (uint64_t i = 0; i < gab.eg->len; i++) {
    if (buflen(gab, kGAB_BUF_DEC, i, e) < cGAB_GC_MOD_BUFF_MAX &&
        buflen(gab, kGAB_BUF_INC, i, e) < cGAB_GC_MOD_BUFF_MAX)
      continue;

#if cGAB_LOG_GC
    printf("TRIGGERED: %i\n", gab.eg->gc->schedule);
#endif

    gab_collect(gab);
    break;
  }

  return true;
}

void gab_gcdocollect(struct gab_triple gab) {
  assert(gab.wkid == 0);
  int32_t epoch = epochget(gab);
  int32_t last = epochgetlast(gab);

  assert(epoch != last);

  processepoch(gab, epoch);

#if cGAB_LOG_GC
  int32_t expected_e = (gab.eg->jobs[gab.wkid].epoch) % 3;
  assert_workers_have_epoch(gab, expected_e);
  printf("CEPOCH %i (last: %i, raw: %i)\n", epoch, last, gab.eg->jobs[gab.wkid].epoch);
#endif

  if (gab_valiso(gab.eg->messages))
    inc_obj_ref(gab, gab_valtoo(gab.eg->messages));

  if (gab_valiso(gab.eg->shapes))
    inc_obj_ref(gab, gab_valtoo(gab.eg->shapes));

  processincrements(gab, epoch);

  processdecrements(gab, last);

  collect_dead(gab);

#if cGAB_LOG_GC
  printf("CEPOCH! %i\n", epochget(gab));
  expected_e = (gab.eg->jobs[gab.wkid].epoch) % 3;
  assert_workers_have_epoch(gab, expected_e);
#endif

  if (gab_valiso(gab.eg->messages))
    queue_decrement(gab, gab_valtoo(gab.eg->messages));

  if (gab_valiso(gab.eg->shapes))
    queue_decrement(gab, gab_valtoo(gab.eg->shapes));

  gab.eg->gc->schedule = -1;
}

void gab_collect(struct gab_triple gab) {
  if (gab.eg->gc->schedule >= 0)
    return;

  schedule(gab, 1);
}
