#include "include/gab.h"
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

enum {
  FLAG_FIBER_DONE = 1 << 0,
  FLAG_FIBER_WAITING = 1 << 1,
  FLAG_FIBER_RUNNING = 1 << 2,
};

typedef struct {
  atomic_char status;
  atomic_int rc;
  v_s_i8 queue;
  mtx_t mutex;
  thrd_t parent;

  gab_value init;
  gab_engine *gab;

  gab_value final;
} fiber;

fiber *fiber_create(gab_value init) {
  fiber *self = NEW(fiber);

  mtx_init(&self->mutex, mtx_plain);
  v_s_i8_create(&self->queue, 8);

  gab_engine *gab = gab_create();

  self->rc = 1;
  self->gab = gab;
  self->init = gab_val_copy(gab, NULL, init);
  self->parent = thrd_current();

  gab_scratch(self->gab, self->init);

  self->status = FLAG_FIBER_RUNNING;

  return self;
}

void fiber_destroy(fiber *self) {
  mtx_destroy(&self->mutex);
  v_s_i8_destroy(&self->queue);
}

boolean callable(gab_value v) {
  return GAB_VAL_IS_BLOCK(v) || GAB_VAL_IS_SUSPENSE(v);
}

i32 fiber_launch(void *d) {
  fiber *self = (fiber *)d;

  gab_value runner = self->init;

  a_gab_value *result = gab_run(self->gab, runner, 0);

  runner = result->data[result->len - 1];

  gab_scratch(self->gab, runner);

  a_gab_value_destroy(result);

  for (;;) {
    if (!callable(runner))
      break;

    mtx_lock(&self->mutex);

    if (self->queue.len == 0) {
      self->status = FLAG_FIBER_WAITING;
      mtx_unlock(&self->mutex);

      thrd_yield();
      continue;
    }

    self->status = FLAG_FIBER_RUNNING;

    s_i8 msg = v_s_i8_pop(&self->queue);

    mtx_unlock(&self->mutex);

    gab_value arg = GAB_VAL_OBJ(gab_obj_string_create(self->gab, msg));
    gab_args(self->gab, 1, &arg, &arg);

    a_gab_value *result = gab_run(self->gab, runner, 0);

    runner = result->data[result->len - 1];

    if (callable(runner))
      gab_scratch(self->gab, runner);

    a_gab_value_destroy(result);
  }

  mtx_lock(&self->mutex);
  self->final = runner;
  self->status = FLAG_FIBER_DONE;
  mtx_unlock(&self->mutex);

  gab_destroy(self->gab);

  return 0;
}

void gab_lib_fiber(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_go");
    return;
  }

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

  gab_value fiber = GAB_VAL_OBJ(
      gab_obj_container_create(gab, vm, GAB_STRING("Fiber"), NULL, NULL, f));

  gab_push(vm, 1, &fiber);
}

void gab_lib_send(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  gab_obj_container *container = GAB_VAL_TO_CONTAINER(argv[0]);

  fiber *f = (fiber *)container->data;

  gab_value msg = gab_val_to_string(gab, argv[1]);

  if (f->status == FLAG_FIBER_DONE) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  mtx_lock(&f->mutex);
  v_s_i8_push(&f->queue, gab_obj_string_ref(GAB_VAL_TO_STRING(msg)));
  mtx_unlock(&f->mutex);
}

void gab_lib_await(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *container = GAB_VAL_TO_CONTAINER(argv[0]);

  fiber *f = (fiber *)container->data;

  while (f->status != FLAG_FIBER_DONE)
    thrd_yield();

  gab_value result = gab_val_copy(gab, vm, f->final);

  gab_push(vm, 1, &result);

  gab_val_dref(vm, result);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("fiber"),
      GAB_STRING("send"),
      GAB_STRING("await"),
  };

  gab_value receivers[] = {
      GAB_VAL_NIL(),
      GAB_STRING("Fiber"),
      GAB_STRING("Fiber"),
  };

  gab_value specs[] = {
      GAB_BUILTIN(fiber),
      GAB_BUILTIN(send),
      GAB_BUILTIN(await),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
