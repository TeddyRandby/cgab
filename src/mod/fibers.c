#include "gab.h"

a_gab_value *gab_lib_fiber(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  gab_value block = gab_arg(1);

  if (!gab_egvalisa(gab.eg, block, gab_egtype(gab.eg, kGAB_BLOCK)))
    return gab_pktypemismatch(gab, block, kGAB_BLOCK);

  gab_value fib = gab_arun(gab, (struct gab_run_argt){
                   .main = block,
               });

  gab_vmpush(gab_vm(gab), fib);

  return nullptr;
}

a_gab_value *gab_lib_channel(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {

  gab_value chan = gab_channel(gab, 0);

  gab_vmpush(gab_vm(gab), chan);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value fiber_t = gab_string(gab, "gab.fiber");
  gab_value channel_t = gab_string(gab, "gab.channel");

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_strtosig(fiber_t),
          gab_snative(gab, "gab.fiber", gab_lib_fiber),
      },
      {
          mGAB_CALL,
          gab_strtosig(channel_t),
          gab_snative(gab, "gab.channel", gab_lib_channel),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return a_gab_value_one(fiber_t);
}
