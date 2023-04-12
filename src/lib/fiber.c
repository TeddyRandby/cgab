#include "include/gab.h"
#include <threads.h>

typedef struct {
  v_s_i8 queue;
  mtx_t mutex;

  gab_value init;
  gab_engine *gab;

} fiber;

fiber *fiber_create(gab_value init) {
  fiber *self = NEW(fiber);

  mtx_init(&self->mutex, mtx_plain);
  v_s_i8_create(&self->queue, 8);

  gab_engine *gab = gab_create();
  self->gab = gab;

  self->init = gab_val_copy(gab, NULL, init);

  gab_scratch(self->gab, self->init);

  return self;
}

void fiber_destroy(fiber *self) {
  mtx_destroy(&self->mutex);
  v_s_i8_destroy(&self->queue);
}

void fiber_destructor(void *self) { fiber_destroy((fiber *)self); }

boolean callable(gab_value v) {
  return GAB_VAL_IS_BLOCK(v) || GAB_VAL_IS_SUSPENSE(v);
}

i32 fiber_launch(void *d) {
  fiber *self = (fiber *)d;

  gab_value runner = self->init;

  runner = gab_run(self->gab, runner, 0);
  gab_scratch(self->gab, runner);

  for (;;) {
    if (!callable(runner))
      break;

    mtx_lock(&self->mutex);

    if (self->queue.len == 0) {
      mtx_unlock(&self->mutex);
      thrd_yield();
      continue;
    }

    s_i8 msg = v_s_i8_pop(&self->queue);
    mtx_unlock(&self->mutex);

    gab_value arg = GAB_VAL_OBJ(gab_obj_string_create(self->gab, msg));
    gab_args(self->gab, 1, &arg, &arg);

    runner = gab_run(self->gab, runner, 0);
    gab_scratch(self->gab, runner);
  }

  gab_destroy(self->gab);

  return 0;
}

void gab_lib_go(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
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

  gab_value fiber = GAB_VAL_OBJ(gab_obj_container_create(
      gab, vm, GAB_STRING("Fiber"), fiber_destructor, NULL, f));

  gab_push(vm, 1, &fiber);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {GAB_STRING("go")};

  gab_value receivers[] = {GAB_VAL_NIL()};

  gab_value specs[] = {GAB_BUILTIN(go)};

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }
  return GAB_VAL_NIL();
}
