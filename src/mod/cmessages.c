#include "gab.h"

a_gab_value *gab_lib_message(struct gab_triple gab, size_t argc,
                             gab_value argv[static argc]) {
  gab_value name = gab_arg(1);

  if (gab_valkind(name) == kGAB_SIGIL)
    name = gab_sigtostr(name);

  if (gab_valkind(name) != kGAB_STRING)
    return gab_pktypemismatch(gab, name, kGAB_STRING);

  gab_vmpush(gab.vm, gab_message(gab, name));
  return nullptr;
}

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[static argc]) {
  switch (argc) {
  case 2: {

    struct gab_egimpl_rest res = gab_egimpl(gab.eg, argv[0], argv[1]);

    gab_vmpush(gab.vm, gab_bool(res.status));
    return nullptr;
  }
  default:
    return gab_panic(gab, "INVALID_ARGUMENTS");
  }
}

a_gab_value *gab_lib_name(struct gab_triple gab, size_t argc,
                          gab_value argv[static argc]) {
  gab_value m = gab_arg(0);

  gab_vmpush(gab.vm, gab_msgname(m));

  return nullptr;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[static argc]) {
  gab_value m = gab_arg(0);
  gab_value r = gab_arg(1);
  gab_value b = gab_arg(2);

  if (gab_valkind(b) != kGAB_PRIMITIVE && gab_valkind(b) != kGAB_BLOCK &&
      gab_valkind(b) != kGAB_NATIVE)
    return gab_pktypemismatch(gab, b, kGAB_BLOCK);

  if (gab_msgput(gab, m, r, b) == gab_undefined)
    return gab_panic(gab, "$ already specializes for type $", m, r);

  return nullptr;
}

a_gab_value *gab_lib_def(struct gab_triple gab, size_t argc,
                         gab_value argv[static argc]) {
  gab_value m = gab_arg(0);
  gab_value r = gab_arg(1);
  gab_value b = gab_arg(2);

  if (gab_valkind(m) != kGAB_MESSAGE)
    return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

  if (gab_valkind(r) != kGAB_RECORD)
    return gab_pktypemismatch(gab, r, kGAB_RECORD);

  size_t len = gab_reclen(r);

  if (len == 0) {
    gab_value t = gab_undefined;

    if (gab_msgput(gab, m, t, b) == gab_undefined)
      return gab_panic(gab, "$ already specializes for type $", m, t);

    return nullptr;
  }

  for (int i = 0; i < len; i++) {
    gab_value t = gab_urecat(r, i);

    if (gab_msgput(gab, m, t, b) == gab_undefined)
      return gab_panic(gab, "$ already specializes for type $", m, t);
  }

  return nullptr;
}

a_gab_value *gab_lib_case(struct gab_triple gab, size_t argc,
                          gab_value argv[static argc]) {
  gab_value m = gab_arg(0);
  gab_value cases = gab_arg(1);

  if (gab_valkind(m) != kGAB_MESSAGE)
    return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

  if (gab_valkind(cases) != kGAB_RECORD)
    return gab_pktypemismatch(gab, cases, kGAB_RECORD);

  for (int i = 0; i < gab_reclen(cases); i++) {
    gab_value b = gab_urecat(cases, i);

    gab_value t = gab_ushpat(gab_recshp(cases), i);

    if (gab_msgput(gab, m, t, b) == gab_undefined)
      return gab_panic(gab, "$ already specializes for type $", m, t);
  }

  return nullptr;
}

a_gab_value *gab_lib_module(struct gab_triple gab, size_t argc,
                          gab_value argv[static argc]) {
  gab_value cases = gab_arg(0);
  gab_value messages = gab_arg(1);

  if (gab_valkind(cases) != kGAB_RECORD)
    return gab_pktypemismatch(gab, cases, kGAB_RECORD);

  if (gab_reclen(cases) == 0) {
    gab_value t = gab_undefined;

    for (int i = 0; i < gab_reclen(messages); i++) {
      gab_value b = gab_urecat(messages, i);

      gab_value m = gab_ushpat(gab_recshp(messages), i);

      if (gab_valkind(m) != kGAB_MESSAGE)
        return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

      if (gab_msgput(gab, m, t, b) == gab_undefined)
        return gab_panic(gab, "$ already specializes for type $", m, t);
    }

    return nullptr;
  }

  for (int j = 0; j < gab_reclen(cases); j++) {
    gab_value t = gab_urecat(cases, j);

    for (int i = 0; i < gab_reclen(messages); i++) {
      gab_value b = gab_urecat(messages, i);

      gab_value m = gab_ushpat(gab_recshp(messages), i);

      if (gab_valkind(messages) != kGAB_RECORD)
        return gab_pktypemismatch(gab, m, kGAB_RECORD);

      if (gab_msgput(gab, m, t, b) == gab_undefined)
        return gab_panic(gab, "$ already specializes for type $", m, t);
    }
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_type(gab.eg, kGAB_MESSAGE);

  struct gab_spec_argt specs[] = {
      {
          "make",
          gab_strtosig(type),
          gab_snative(gab, "message.new", gab_lib_message),
      },
      {
          "name",
          type,
          gab_snative(gab, "name", gab_lib_name),
      },
      {
          "has?",
          type,
          gab_snative(gab, "has?", gab_lib_at),
      },
      {
          "put!",
          type,
          gab_snative(gab, "put!", gab_lib_put),
      },
      {
          "def!",
          type,
          gab_snative(gab, "def!", gab_lib_def),
      },
      {
          "defcase!",
          type,
          gab_snative(gab, "defcase!", gab_lib_case),
      },
      {
          "defmodule!",
          gab_undefined,
          gab_snative(gab, "defmodule!", gab_lib_module),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return a_gab_value_one(type);
}
