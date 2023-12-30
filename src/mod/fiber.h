#include "gab.h"
#include <threads.h>

enum {
  fDONE,
  fWAITING,
  fRUNNING,
};

struct fiber {
  atomic_char status;
  atomic_int rc;
  thrd_t thrd;
  thrd_t parent;

  gab_value runner;
  struct gab_triple gab;
};

void fiber_destroy(size_t len, unsigned char data[static len]) {
  struct fiber *f = (struct fiber *)data;
  if (!f->rc--) {
    gab_destroy(f->gab);
  }
}

gab_value fiber_create(struct gab_triple gab, gab_value runner) {
  assert(gab_valkind(runner) == kGAB_SUSPENSE || gab_valkind(runner) == kGAB_BLOCK);

  struct fiber f = {
    .rc = 1,
    .gab = gab_create(),
    .parent = thrd_current(),
    .runner = runner,
  };

  gab_gciref(gab, f.runner);
  gab_egkeep(f.gab.eg, f.runner);

  gab_value fiber = gab_box(gab, (struct gab_box_argt){
                                      .data = &f,
                                      .size = sizeof(struct fiber),
                                      .type = gab_string(gab, "Fiber"),
                                      .destructor = fiber_destroy,
                                  });

  return fiber;
}

void fiber_run(struct fiber *f) {
  f->status = fRUNNING;

  a_gab_value *result =
      gab_run(f->gab, (struct gab_run_argt){
                          .main = f->runner,
                          .flags = fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC,
                      });
  printf("RUNNING %V\n", f->runner);

  f->runner = result->data[result->len - 1];
  free(result);

  enum gab_kind runk = gab_valkind(f->runner);

  if (runk != kGAB_BLOCK && runk != kGAB_SUSPENSE)
    f->status = fDONE;
  else
    f->status = fWAITING;

}

int fiber_launch(void *d) {
  gab_value fiber = (gab_value)(intptr_t)d;
  struct fiber *f = gab_boxdata(fiber);

  f->rc++;

  fiber_run(f);

  fiber_destroy(sizeof(struct fiber), (void *)f);

  return 0;
}

bool fiber_go(gab_value fiber) {
    struct fiber *f = gab_boxdata(fiber);

    if (thrd_create(&f->thrd, fiber_launch, (void*) (intptr_t) fiber) != thrd_success) {
      return false;
    }

    thrd_detach(f->thrd);
    return true;
}
