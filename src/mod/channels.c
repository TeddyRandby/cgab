#include "channel.h"
#include "gab.h"

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

a_gab_value *gab_lib_empty(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  bool res = channel_isempty(gab_arg(0));
  gab_vmpush(gab.vm, gab_bool(res));
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
          "empty?",
          channel_t,
          gab_snative(gab, "empty?", gab_lib_empty),
      },
      {
          "<-",
          channel_t,
          gab_snative(gab, "send", gab_lib_send),
      },
      {
          "->",
          channel_t,
          gab_snative(gab, "receive", gab_lib_recv),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return nullptr;
}
