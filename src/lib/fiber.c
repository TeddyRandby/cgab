#include "include/core.h"
#include "include/gab.h"
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

enum {
  fDONE = 1 << 0,
  fWAITING = 1 << 1,
  fRUNNING = 1 << 2,
};

typedef struct {
  atomic_char status;
  atomic_int rc;
  v_a_i8 in_queue;
  v_a_i8 out_queue;
  mtx_t mutex;
  thrd_t parent;

  gab_value init;
  gab_eg *gab;

  a_gab_value *final;
} fiber;

fiber *fiber_create(gab_value init) {
  fiber *self = NEW(fiber);

  mtx_init(&self->mutex, mtx_plain);
  v_a_i8_create(&self->in_queue, 8);
  v_a_i8_create(&self->out_queue, 8);

  gab_eg *gab = gab_create(0, NULL, NULL);

  self->rc = 1;
  self->gab = gab;
  self->init = gab_valcpy(gab, NULL, init);
  self->parent = thrd_current();

  gab_egkeep(self->gab, self->init);

  self->status = fRUNNING;

  return self;
}

void fiber_destroy(fiber *self) {
  mtx_destroy(&self->mutex);
  v_a_i8_destroy(&self->in_queue);
  v_a_i8_destroy(&self->out_queue);
}

boolean callable(gab_value v) {
  return gab_valknd(v) == kGAB_BLOCK || gab_valknd(v) == kGAB_SUSPENSE;
}

i32 fiber_launch(void *d) {
  fiber *self = (fiber *)d;

  gab_value runner = self->init;

  a_gab_value *result =
      gab_run(self->gab, runner, fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC);

  if (!result) {
    mtx_lock(&self->mutex);
    self->final = a_gab_value_one(gab_nil);
    self->status = fDONE;
    mtx_unlock(&self->mutex);
    return 0;
  }

  runner = result->data[result->len - 1];

  if (!callable(runner)) {
    mtx_lock(&self->mutex);

    self->final = result;
    self->status = fDONE;

    mtx_unlock(&self->mutex);

    gab_free(self->gab);
    return 0;
  }

  gab_egkeep(self->gab, runner);
  a_gab_value_destroy(result);

  for (;;) {
    mtx_lock(&self->mutex);

    if (self->in_queue.len == 0) {
      self->status = fWAITING;
      mtx_unlock(&self->mutex);

      thrd_yield();
      continue;
    }

    self->status = fRUNNING;

    a_i8 *msg = v_a_i8_pop(&self->in_queue);

    mtx_unlock(&self->mutex);

    gab_value arg = gab_nstring(self->gab, msg->len, (char *)msg->data);

    a_i8_destroy(msg);

    gab_argput(self->gab, arg, 1);

    a_gab_value *result = gab_run(self->gab, runner, fGAB_DUMP_ERROR);

    runner = result->data[result->len - 1];

    mtx_lock(&self->mutex);
    for (u32 i = 0; i < result->len - 1; i++) {
      gab_egkeep(self->gab, result->data[i]);

      char *ref = gab_valtocs(self->gab, result->data[i]);

      v_a_i8_push(&self->out_queue, a_i8_create((const i8 *)ref, strlen(ref)));
      free(ref);
    }
    mtx_unlock(&self->mutex);

    if (!callable(runner))
      break;

    gab_egkeep(self->gab, runner);
    a_gab_value_destroy(result);
  }

  mtx_lock(&self->mutex);
  self->final = result;
  self->status = fDONE;
  mtx_unlock(&self->mutex);

  gab_free(self->gab);

  return 0;
}

void gab_lib_fiber(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (!callable(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_go");
      return;
    }

    fiber *f = fiber_create(argv[1]);
    thrd_t thread;

    if (thrd_create(&thread, fiber_launch, f) != thrd_success) {
      gab_panic(gab, vm, "Invalid call to gab_lib_go");
      return;
    }

    gab_value fiber = gab_box(gab, vm,
                              (struct gab_box_argt){
                                  .data = f,
                                  .type = gab_string(gab, "Fiber"),
                                  .visitor = NULL,
                                  .destructor = NULL,
                              });

    gab_vmpush(vm, fiber);
    break;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_go");
    return;
  }
}

void gab_lib_send(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  gab_value msg = gab_valtos(gab, argv[1]);
  fiber *f = (fiber *)gab_boxdata(argv[0]);

  if (f->status == fDONE) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  mtx_lock(&f->mutex);

  char *ref = gab_valtocs(gab, msg);

  v_a_i8_push(&f->in_queue, a_i8_create((char *)ref, strlen(ref)));

  free(ref);

  mtx_unlock(&f->mutex);

  gab_vmpush(vm, *argv);
}

void gab_lib_await(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  fiber *f = gab_boxdata(argv[0]);

  for (;;) {
    if (f->status == fDONE) {
      gab_nvmpush(vm, f->final->len, f->final->data);
      return;
    }

    mtx_lock(&f->mutex);

    if (f->out_queue.len < 1) {
      mtx_unlock(&f->mutex);

      thrd_yield();
      continue;
    }

    break;
  }

  a_i8 *msg = v_a_i8_pop(&f->out_queue);

  mtx_unlock(&f->mutex);

  gab_value result = gab_nstring(gab, msg->len, (char *)msg->data);

  a_i8_destroy(msg);

  gab_vmpush(vm, result);

  gab_gcdref(gab_vmgc(vm), vm, result);
}

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "fiber",
      mGAB_CALL,
      "await",
  };

  gab_value receivers[] = {
      gab_nil,
      gab_string(gab, "Fiber"),
      gab_string(gab, "Fiber"),
  };

  gab_value specs[] = {
      gab_builtin(gab, "fiber", gab_lib_fiber),
      gab_builtin(gab, "send", gab_lib_send),
      gab_builtin(gab, "await", gab_lib_await),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = receivers[i],
                 .specialization = specs[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(specs), receivers);

  return NULL;
}
