#include "gab.h"
#include <threads.h>

enum {
  fNONE,
  fDONE,
  fRUNNING,
};

struct fiber {
  _Atomic char status;
  _Atomic int rc;
  thrd_t thrd;
  thrd_t parent;

  gab_value runner;
  a_gab_value *last_result;
  struct gab_triple gab;
};

void fiber_deref(size_t len, char data[static len]) {
  struct fiber *f = *(struct fiber **)data;
  f->rc--;
  if (f->rc == 0) {
    gab_destroy(f->gab);
    free(f);
  }
}

void fiber_iref(gab_value fiber) {
  assert(gab_valkind(fiber) == kGAB_BOX);
  struct fiber *f = *(struct fiber **)gab_boxdata(fiber);
  f->rc++;
}

gab_value fiber_create(struct gab_triple gab, gab_value runner) {
  assert(gab_valkind(runner) == kGAB_BLOCK);

  struct fiber *f = malloc(sizeof(struct fiber));

  f->rc = 1;
  f->status = fNONE;
  f->gab = gab_create((struct gab_create_argt){
      .os_dynopen = gab.eg->os_dynopen,
      .os_dynsymbol = gab.eg->os_dynsymbol,
  });
  f->parent = thrd_current();
  f->runner = gab_valcpy(f->gab, runner);
  f->last_result = nullptr;

  gab_iref(gab, f->runner);
  gab_egkeep(f->gab.eg, f->runner);

  gab_value fiber = gab_box(gab, (struct gab_box_argt){
                                     .data = &f,
                                     .size = sizeof(struct fiber *),
                                     .type = gab_string(gab, "gab.fiber"),
                                     .destructor = fiber_deref,
                                 });

  return fiber;
}

void fiber_run(gab_value fiber) {
  assert(gab_valkind(fiber) == kGAB_BOX);
  struct fiber *f = *(struct fiber **)gab_boxdata(fiber);

  if (f->status == fDONE || f->status == fRUNNING) {
    goto fin;
  }

  f->status = fRUNNING;

  a_gab_value *result = gab_run(f->gab, (struct gab_run_argt){
                                            .main = f->runner,
                                            .flags = fGAB_ERR_QUIET,
                                        });

  /* Decrement all the previous results */
  if (f->last_result) {
    gab_ndref(f->gab, 1, f->last_result->len, f->last_result->data);
    free(f->last_result);
  }

  f->last_result = result;
  f->status = fDONE;

fin:
  fiber_deref(sizeof(struct fiber *), (char *)&f);
}

int fiber_launch(void *d) {
  gab_value fiber = (gab_value)(intptr_t)d;

  fiber_run(fiber);

  return 0;
}

bool fiber_go(gab_value fiber) {
  assert(gab_valkind(fiber) == kGAB_BOX);
  struct fiber *f = *(struct fiber **)gab_boxdata(fiber);

  if (thrd_create(&f->thrd, fiber_launch, (void *)(intptr_t)fiber) !=
      thrd_success) {
    return false;
  }

  thrd_detach(f->thrd);
  return true;
}

a_gab_value *gab_lib_fiber(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  gab_value block = gab_arg(1);

  if (gab_valkind(block) != kGAB_BLOCK)
    return gab_pktypemismatch(gab, block, kGAB_BLOCK);

  gab_value fiber = fiber_create(gab, argv[1]);

  gab_vmpush(gab.vm, fiber);

  fiber_iref(fiber);

  if (!fiber_go(fiber)) {
    return gab_panic(gab, "Failed to start fiber");
  }

  return nullptr;
}

a_gab_value *gab_lib_call(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  struct fiber *f = *(struct fiber **)gab_boxdata(argv[0]);

  if (f->status == fDONE) {
    fiber_iref(argv[0]);
    fiber_run(argv[0]);

    gab_vmpush(gab.vm, gab_ok, argv[0]);
    return nullptr;
  }

  return nullptr;
}

a_gab_value *gab_lib_await(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  struct fiber *f = *(struct fiber **)gab_boxdata(argv[0]);

  for (;;) {
    if (f->status == fDONE) {
      gab_nvmpush(gab.vm, f->last_result->len, f->last_result->data);
      return nullptr;
    }
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value fiber_t = gab_string(gab, "gab.fiber");

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_strtosig(fiber_t),
          gab_snative(gab, "gab.fiber", gab_lib_fiber),
      },
      {
          mGAB_CALL,
          fiber_t,
          gab_snative(gab, mGAB_CALL, gab_lib_call),
      },
      {
          "await",
          fiber_t,
          gab_snative(gab, "await", gab_lib_await),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return nullptr;
}
