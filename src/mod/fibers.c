#include "fiber.h"
#include "gab.h"

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
