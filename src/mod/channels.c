#include "channel.h"

a_gab_value *gab_lib_channel(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  gab_value c = channel_create(gab);
  channel_iref(c);
  gab_vmpush(gab.vm, c);
  return nullptr;
}

a_gab_value *gab_lib_send(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  channel_send(gab, argv[0], argc - 1, argv + 1);
  return nullptr;
}

a_gab_value *gab_lib_recv(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value v = channel_recv(gab, argv[0]);
  if (v == gab_undefined) {
    gab_vmpush(gab.vm, gab_none);
  } else {
    gab_vmpush(gab.vm, gab_ok, v);
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value channel_t = gab_string(gab, "gab.channel");

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_strtosig(channel_t),
          gab_snative(gab, "gab.channel", gab_lib_channel),
      },
      {
          mGAB_LSH,
          channel_t,
          gab_snative(gab, mGAB_CALL, gab_lib_send),
      },
      {
          "send",
          channel_t,
          gab_snative(gab, mGAB_CALL, gab_lib_send),
      },
      {
          "await",
          channel_t,
          gab_snative(gab, "await", gab_lib_recv),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return nullptr;
}
