#include "channel.h"

a_gab_value * gab_lib_channel(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  gab_value c = channel_create(gab);
  gab_vmpush(gab.vm, c);
  return NULL;
}

a_gab_value * gab_lib_send(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  channel_send(gab, argv[0], argc - 1, argv + 1);
  return NULL;
}

a_gab_value *gab_lib_recv(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value v = channel_recv(gab, argv[0]);
  if (v == gab_undefined) {
    gab_vmpush(gab.vm, gab_string(gab, "none"));
  } else {
    gab_vmpush(gab.vm, gab_string(gab, "some"));
    gab_vmpush(gab.vm, v);
  }

  return NULL;
}

a_gab_value *gab_lib(struct gab_triple gab) {

  struct gab_spec_argt specs[] = {
      {
          "channel.new",
          gab_undefined,
          gab_snative(gab, "channel.new", gab_lib_channel),
      },
      {
          mGAB_LSH,
          gab_string(gab, "Channel"),
          gab_snative(gab, mGAB_CALL, gab_lib_send),
      },
      {
          "send",
          gab_string(gab, "Channel"),
          gab_snative(gab, mGAB_CALL, gab_lib_send),
      },
      {
          "await",
          gab_string(gab, "Channel"),
          gab_snative(gab, "await", gab_lib_recv),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return NULL;
}
