#include "gab.h"
#include <threads.h>

enum {
  fNONE,
  fDONE,
  fPAUSED,
  fRUNNING,
};

struct fiber {
  _Atomic char status;
  _Atomic int rc;
  thrd_t thrd;
  thrd_t parent;

  gab_value runner;
  struct gab_triple gab;
};

void fiber_deref(size_t len, unsigned char data[static len]) {
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
  assert(gab_valkind(runner) == kGAB_SUSPENSE ||
         gab_valkind(runner) == kGAB_BLOCK);

  struct fiber *f = malloc(sizeof(struct fiber));

  f->rc = 1;
  f->status = fNONE;
  f->gab = gab_create();
  f->parent = thrd_current();
  f->runner = gab_valcpy(f->gab, runner);

  gab_iref(gab, f->runner);
  gab_egkeep(f->gab.eg, f->runner);

  gab_value fiber = gab_box(gab, (struct gab_box_argt){
                                     .data = &f,
                                     .size = sizeof(struct fiber *),
                                     .type = gab_string(gab, "Fiber"),
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

  a_gab_value *result =
      gab_run(f->gab, (struct gab_run_argt){
                          .main = f->runner,
                          .flags = fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC,
                      });

  f->runner = result->data[result->len - 1];

  enum gab_kind runk = gab_valkind(f->runner);
  bool continuing = runk == kGAB_BLOCK || runk == kGAB_SUSPENSE;

  if (continuing)
    gab_egkeep(f->gab.eg, f->runner);

  /* Decrement all the results we didn't use. */
  gab_ndref(f->gab, 1, result->len - continuing, result->data + continuing);

  free(result);

  f->status = continuing ? fPAUSED : fDONE;

fin:
  fiber_deref(sizeof(struct fiber *), (unsigned char *)&f);
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
