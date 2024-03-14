#include "fiber.h"
#include "core.h"
#include "gab.h"
#include <threads.h>

a_gab_value *gab_lib_fiber(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    enum gab_kind runk = gab_valkind(argv[1]);
    if (runk != kGAB_BLOCK) {
      return gab_panic(gab, "Invalid call to &:fiber.new");
    }

    gab_value fiber = fiber_create(gab, argv[1]);

    gab_vmpush(gab.vm, fiber);

    fiber_iref(fiber);

    if (!fiber_go(fiber)) {
      return gab_panic(gab, "Failed to start fiber");
    }

    break;
  }
  default:
    return gab_panic(gab, "Invalid call to &:fiber.new");
  }

  return NULL;
}

a_gab_value *gab_lib_call(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  struct fiber *f = *(struct fiber **)gab_boxdata(argv[0]);

  if (f->status == fDONE) {
    gab_vmpush(gab.vm, gab_string(gab, "none"));
    return NULL;
  }

  if (f->status == fPAUSED) {
    fiber_iref(argv[0]);
    fiber_run(argv[0]);

    gab_vmpush(gab.vm, gab_string(gab, "some"));
    gab_vmpush(gab.vm, argv[0]);
    return NULL;
  }

  return NULL;
}

a_gab_value *gab_lib_await(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  struct fiber *f = *(struct fiber **)gab_boxdata(argv[0]);

  for (;;) {
    if (f->status == fDONE) {
      return NULL;
    }
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {

  struct gab_spec_argt specs[] = {
      {
          "fiber.new",
          gab_undefined,
          gab_snative(gab, "fiber.new", gab_lib_fiber),
      },
      {
          mGAB_CALL,
          gab_string(gab, "Fiber"),
          gab_snative(gab, mGAB_CALL, gab_lib_call),
      },
      {
          "await",
          gab_string(gab, "Fiber"),
          gab_snative(gab, "await", gab_lib_await),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return NULL;
}
