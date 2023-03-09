#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>
#include <threads.h>

typedef struct {
  boolean started;

  gab_engine *gab;

  gab_value proc;

  mtx_t mtx;

  thrd_t thrd;

  v_gab_value queue;
} fiber;

fiber *fiber_create(gab_engine *gab, gab_value proc) {
  fiber *self = NEW(fiber);

  self->started = false;

  self->gab = gab;

  self->proc = proc;

  mtx_init(&self->mtx, mtx_plain);

  v_gab_value_create(&self->queue, 8);

  return self;
}

boolean fiber_empty(gab_engine *gab, fiber *f) { return f->queue.len == 0; }

void fiber_push(fiber *f, gab_value msg) {
  mtx_lock(&f->mtx);

  v_gab_value_push(&f->queue, gab_val_copy(f->gab, msg));

  mtx_unlock(&f->mtx);
}

gab_value fiber_pop(fiber *f) {
  mtx_lock(&f->mtx);

  gab_value msg = v_gab_value_del(&f->queue, 0);

  mtx_unlock(&f->mtx);

  gab_dref(f->gab, NULL, msg);

  return msg;
}

void fiber_set(fiber *f, gab_value v) {
  mtx_lock(&f->mtx);

  f->proc = v;

  mtx_unlock(&f->mtx);
}

boolean callable(gab_value v) {
  return GAB_VAL_IS_BLOCK(v) || GAB_VAL_IS_SUSPENSE(v);
}

i32 fiber_run(void *arg) {
  fiber *f = arg;

  gab_engine *gab = f->gab;

  gab_args(gab, 0, NULL, NULL);
  gab_value res = gab_run(gab, f->proc, GAB_FLAG_DUMP_ERROR);

  if (!callable(res))
    return 1;

  fiber_set(f, res);

  gab_value empty = GAB_STRING("");

  while (callable(f->proc)) {
    if (!fiber_empty(gab, f)) {
      gab_value msg = fiber_pop(f);

      gab_args(gab, 1, &empty, &msg);

      res = gab_run(gab, f->proc, GAB_FLAG_DUMP_ERROR);

      if (GAB_VAL_IS_UNDEFINED(res)) {
        return 0;
      }

      fiber_set(f, res);
    }
  }

  return 1;
}

gab_value gab_fiber_create(gab_engine *gab, gab_vm *vm, fiber *f) {
  gab_obj_container *k = gab_obj_container_create(gab, GAB_STRING("Fiber"), f);

  if (!f->started) {
    f->started = true;

    if (thrd_create(&f->thrd, fiber_run, f) != thrd_success)
      return gab_panic(gab, vm, "Failed to create system thread");
  }

  return GAB_VAL_OBJ(k);
}

gab_value gab_lib_cal(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc < 2)
    return gab_panic(gab, vm, "Invalid call to gab_lib_cal");

  gab_obj_container *k = GAB_VAL_TO_CONTAINER(argv[0]);

  fiber *f = k->data;

  for (u8 i = 1; i < argc; i++) {
    fiber_push(f, argv[i]);
  }

  return GAB_VAL_NIL();
}

gab_value gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 2)
    return gab_panic(gab, vm, "Invalid call to gab_lib_new");

  if (!GAB_VAL_IS_BLOCK(argv[1]))
    return gab_panic(gab, vm, "Invalid call to gab_lib_new");

  gab_engine *fork = gab_create(gab_seed(gab));

  fiber *f = fiber_create(fork, gab_val_copy(fork, argv[1]));

  return gab_fiber_create(gab, vm, f);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value fiber =
      GAB_VAL_OBJ(gab_obj_symbol_create(gab, GAB_STRING("Fiber")));
  gab_dref(gab, vm, fiber);

  gab_value keys[] = {
      GAB_STRING("new"),
      GAB_STRING(GAB_MESSAGE_CAL),
  };

  gab_value receiver_types[] = {
      fiber,
      gab_type(gab, GAB_KIND_CONTAINER),
  };

  gab_value values[] = {
      GAB_BUILTIN(new),
      GAB_BUILTIN(cal),
  };

  for (u8 i = 0; i < 3; i++) {
    gab_specialize(gab, vm, keys[i], receiver_types[i], values[i]);
    gab_dref(gab, vm, values[i]);
  }

  return fiber;
}
