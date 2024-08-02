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

a_gab_value *gab_lib_has(struct gab_triple gab, size_t argc,
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

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[static argc]) {
  gab_value m = gab_arg(0);
  gab_value k = gab_arg(1);

  struct gab_egimpl_rest res = gab_egimpl(gab.eg, m, k);

  gab_value values[] = {gab_none, gab_nil};

  if (res.status) {
    values[0] = gab_ok;
    values[1] = res.spec;
  }

  gab_nvmpush(gab.vm, 2, values);
  return nullptr;
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

  if (gab_valkind(r) != kGAB_MAP)
    return gab_pktypemismatch(gab, r, kGAB_MAP);

  size_t len = gab_maplen(r);

  if (len == 0) {
    gab_value t = gab_undefined;

    if (gab_msgput(gab, m, t, b) == gab_undefined)
      return gab_panic(gab, "$ already specializes for type $", m, t);

    return nullptr;
  }

  for (size_t i = 0; i < len; i++) {
    gab_value t = gab_uvmapat(r, i);

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

  if (gab_valkind(cases) != kGAB_MAP)
    return gab_pktypemismatch(gab, cases, kGAB_MAP);

  for (int i = 0; i < gab_maplen(cases); i++) {
    gab_value b = gab_uvmapat(cases, i);
    gab_value t = gab_ukmapat(cases, i);

    if (gab_msgput(gab, m, t, b) == gab_undefined)
      return gab_panic(gab, "$ already specializes for type $", m, t);
  }

  return nullptr;
}

a_gab_value *gab_lib_module(struct gab_triple gab, size_t argc,
                            gab_value argv[static argc]) {
  gab_value cases = gab_arg(0);
  gab_value messages = gab_arg(1);

  if (gab_valkind(cases) != kGAB_MAP)
    return gab_pktypemismatch(gab, cases, kGAB_MAP);

  if (gab_valkind(messages) != kGAB_MAP)
    return gab_pktypemismatch(gab, messages, kGAB_MAP);

  if (gab_maplen(cases) == 0) {
    gab_value t = gab_undefined;

    for (size_t i = 0; i < gab_maplen(messages); i++) {
      gab_value b = gab_uvmapat(messages, i);
      gab_value m = gab_ukmapat(messages, i);

      if (gab_valkind(m) != kGAB_MESSAGE)
        return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

      if (gab_msgput(gab, m, t, b) == gab_undefined)
        return gab_panic(gab, "$ already specializes for type $", m, t);
    }

    return nullptr;
  }

  for (int j = 0; j < gab_maplen(cases); j++) {
    gab_value t = gab_uvmapat(cases, j);

    for (int i = 0; i < gab_maplen(messages); i++) {
      gab_value b = gab_uvmapat(messages, i);

      gab_value m = gab_ukmapat(messages, i);

      if (gab_valkind(messages) != kGAB_MAP)
        return gab_pktypemismatch(gab, m, kGAB_MAP);

      if (gab_msgput(gab, m, t, b) == gab_undefined)
        return gab_panic(gab, "$ already specializes for type $", m, t);
    }
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_egtype(gab.eg, kGAB_MESSAGE);

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
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
          gab_snative(gab, "has?", gab_lib_has),
      },
      {
          "at",
          type,
          gab_snative(gab, "at", gab_lib_at),
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
