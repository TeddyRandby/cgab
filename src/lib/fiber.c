#include "include/core.h"
#include "include/gab.h"
#include "include/value.h"
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
  gab_engine *gab;

  a_gab_value *final;
} fiber;

fiber *fiber_create(gab_value init) {
  fiber *self = NEW(fiber);

  mtx_init(&self->mutex, mtx_plain);
  v_a_i8_create(&self->in_queue, 8);
  v_a_i8_create(&self->out_queue, 8);

  gab_engine *gab = gab_create();

  self->rc = 1;
  self->gab = gab;
  self->init = gab_val_copy(gab, NULL, init);
  self->parent = thrd_current();

  gab_scratch(self->gab, self->init);

  self->status = fRUNNING;

  return self;
}

void fiber_destroy(fiber *self) {
  mtx_destroy(&self->mutex);
  v_a_i8_destroy(&self->in_queue);
  v_a_i8_destroy(&self->out_queue);
}

boolean callable(gab_value v) {
  return GAB_VAL_IS_BLOCK(v) || GAB_VAL_IS_SUSPENSE(v);
}

i32 fiber_launch(void *d) {
  fiber *self = (fiber *)d;

  gab_value runner = self->init;

  a_gab_value *result =
      gab_run(self->gab, runner, fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC);

  if (!result) {
    mtx_lock(&self->mutex);
    self->final = a_gab_value_one(GAB_VAL_NIL());
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

    gab_destroy(self->gab);
    return 0;
  }

  gab_scratch(self->gab, runner);
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

    gab_value arg =
        GAB_VAL_OBJ(gab_obj_string_create(self->gab, s_i8_arr(msg)));

    a_i8_destroy(msg);

    gab_args(self->gab, 1, &arg, &arg);

    a_gab_value *result = gab_run(self->gab, runner, fGAB_DUMP_ERROR);

    runner = result->data[result->len - 1];

    mtx_lock(&self->mutex);
    for (u32 i = 0; i < result->len - 1; i++) {
      gab_scratch(self->gab, result->data[i]);

      s_i8 ref = gab_obj_string_ref(
          GAB_VAL_TO_STRING(gab_val_to_s(self->gab, result->data[i])));

      v_a_i8_push(&self->out_queue, a_i8_create(ref.data, ref.len));
    }
    mtx_unlock(&self->mutex);

    if (!callable(runner))
      break;

    gab_scratch(self->gab, runner);
    a_gab_value_destroy(result);
  }

  mtx_lock(&self->mutex);
  self->final = result;
  self->status = fDONE;
  mtx_unlock(&self->mutex);

  gab_destroy(self->gab);

  return 0;
}

void gab_lib_fiber(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
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

    gab_value fiber = GAB_VAL_OBJ(
        gab_obj_container_create(gab, vm, GAB_STRING("Fiber"), NULL, NULL, f));

    gab_push(vm, fiber);
    break;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_go");
    return;
  }
}

void gab_lib_send(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  gab_obj_container *container = GAB_VAL_TO_CONTAINER(argv[0]);

  fiber *f = (fiber *)container->data;

  gab_value msg = gab_val_to_s(gab, argv[1]);

  if (f->status == fDONE) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  mtx_lock(&f->mutex);

  s_i8 ref = gab_obj_string_ref(GAB_VAL_TO_STRING(msg));

  v_a_i8_push(&f->in_queue, a_i8_create(ref.data, ref.len));

  mtx_unlock(&f->mutex);

  gab_push(vm, *argv);
}

void gab_lib_await(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *container = GAB_VAL_TO_CONTAINER(argv[0]);

  fiber *f = (fiber *)container->data;

  for (;;) {
    if (f->status == fDONE) {
      gab_vpush(vm, f->final->len, f->final->data);
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

  gab_value result = GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_arr(msg)));

  a_i8_destroy(msg);

  gab_push(vm, result);

  gab_val_dref(vm, result);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("fiber"),
      GAB_STRING(mGAB_CALL),
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
