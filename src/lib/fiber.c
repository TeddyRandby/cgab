#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include <stdio.h>
#include <threads.h>

typedef struct {
  _Atomic i32 rc;

  boolean running;

  gab_engine *p_gab;

  gab_vm *p_vm;

  gab_value val;

  mtx_t mtx;

  thrd_t thrd;

  v_gab_value queue;
} fiber;

fiber *fiber_create(gab_engine *gab, gab_vm *vm, gab_value v) {
  fiber *self = NEW(fiber);

  self->rc = 0;
  self->running = false;

  self->p_gab = gab;
  self->p_vm = vm;

  self->val = v;

  mtx_init(&self->mtx, mtx_plain);

  v_gab_value_create(&self->queue, 8);

  return self;
}

boolean fiber_empty(gab_engine *gab, fiber *f) { return f->queue.len == 0; }

void fiber_push(gab_engine *gab, gab_vm *vm, fiber *f, gab_value msg) {
  mtx_lock(&f->mtx);

  v_gab_value_push(&f->queue, msg);

  mtx_unlock(&f->mtx);

  gab_iref(gab, vm, msg);
}

gab_value fiber_pop(gab_engine *gab, gab_vm *vm, fiber *f) {
  mtx_lock(&f->mtx);

  gab_value msg = v_gab_value_del(&f->queue, 0);

  mtx_unlock(&f->mtx);

  gab_dref(gab, vm, msg);

  return msg;
}

i32 fiber_run(void *arg) {
  fiber *f = arg;

  gab_engine *gab = f->p_gab;

  gab_vm *vm = f->p_vm;

  gab_value p = f->val;

  printf("Forking Engine...");
  gab_engine *fork = gab_fork(gab);
  printf("Done.\n");

  printf("Running: ");
  gab_val_dump(stdout, p);
  printf("\n");

  gab_value p_copy = gab_val_copy(fork, p);

  gab_run(fork, p_copy, GAB_FLAG_DUMP_ERROR);

  // do {
  //   if (!fiber_empty(gab, f)) {
  //     printf("Fiber has message\n");
  //     gab_value msg = fiber_pop(gab, vm, f);
  //
  //     // gab_value m_copy = gab_val_copy(fork, msg);
  //
  //     gab_value p_copy = gab_val_copy(fork, p);
  //
  //     gab_run(fork, p_copy, GAB_FLAG_DUMP_ERROR);
  //   }
  // } while (1);

  return 1;
}

gab_value gab_fiber_create(gab_engine *gab, gab_vm *vm, fiber *f) {
  gab_obj_container *k = gab_obj_container_create(gab, GAB_STRING("Fiber"), f);

  f->rc++;

  if (!f->running) {
    f->running = true;
    if (thrd_create(&f->thrd, fiber_run, f) != thrd_success)
      return gab_panic(gab, vm, "Failed to create system thread");
  }

  return GAB_VAL_OBJ(k);
}

gab_value gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 2)
    return gab_panic(gab, vm, "Invalid call to gab_lib_new");

  if (!GAB_VAL_IS_BLOCK(argv[1]))
    return gab_panic(gab, vm, "Invalid call to gab_lib_new");

  fiber *f = fiber_create(gab, vm, argv[1]);

  return gab_fiber_create(gab, vm, f);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value fiber =
      GAB_VAL_OBJ(gab_obj_symbol_create(gab, GAB_STRING("Fiber")));
  gab_dref(gab, vm, fiber);

  gab_value keys[] = {
      GAB_STRING("new"),
  };

  gab_value receiver_types[] = {
      fiber,
  };

  gab_value values[] = {
      GAB_BUILTIN(new),
  };

  for (u8 i = 0; i < 3; i++) {
    gab_specialize(gab, vm, keys[i], receiver_types[i], values[i]);
    gab_dref(gab, vm, values[i]);
  }

  return fiber;
}
