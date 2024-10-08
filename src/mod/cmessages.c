#include "gab.h"

a_gab_value *gab_lib_message(struct gab_triple gab, size_t argc,
                             gab_value argv[static argc]) {
  gab_value name = gab_arg(1);

  if (gab_valkind(name) == kGAB_SIGIL)
    name = gab_sigtostr(name);

  if (gab_valkind(name) != kGAB_STRING)
    return gab_pktypemismatch(gab, name, kGAB_STRING);

  gab_vmpush(gab_vm(gab), gab_strtomsg(name));
  return nullptr;
}

a_gab_value *gab_lib_impls(struct gab_triple gab, size_t argc,
                           gab_value argv[static argc]) {
  if (argc == 1) {
    gab_value rec = gab_fibmsg(gab_thisfiber(gab));
    gab_vmpush(gab_vm(gab), rec);

    return nullptr;
  }

  gab_value msg = gab_arg(1);
  gab_value rec = gab_fibmsgrec(gab_thisfiber(gab), msg);

  if (rec == gab_undefined)
    gab_vmpush(gab_vm(gab), gab_nil);
  else
    gab_vmpush(gab_vm(gab), rec);

  return nullptr;
}

a_gab_value *gab_lib_has(struct gab_triple gab, size_t argc,
                         gab_value argv[static argc]) {
  switch (argc) {
  case 2: {

    struct gab_impl_rest res = gab_impl(gab, argv[0], argv[1]);

    gab_vmpush(gab_vm(gab), gab_bool(res.status));
    return nullptr;
  }
  default:
    return gab_fpanic(gab, "INVALID_ARGUMENTS");
  }
}

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[static argc]) {
  gab_value m = gab_arg(0);
  gab_value k = gab_arg(1);

  struct gab_impl_rest res = gab_impl(gab, m, k);

  gab_value values[] = {gab_none, gab_nil};

  if (res.status) {
    values[0] = gab_ok;
    values[1] = res.as.spec;
  }

  gab_nvmpush(gab_vm(gab), 2, values);
  return nullptr;
}

a_gab_value *gab_lib_sigil_into(struct gab_triple gab, size_t argc,
                                gab_value argv[static argc]) {
  gab_value m = gab_arg(0);

  gab_vmpush(gab_vm(gab), gab_strtosig(gab_msgtostr(m)));

  return nullptr;
}

a_gab_value *gab_lib_string_into(struct gab_triple gab, size_t argc,
                                 gab_value argv[static argc]) {
  gab_value m = gab_arg(0);

  gab_vmpush(gab_vm(gab), gab_msgtostr(m));

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

  if (gab_fibmsgput(gab, gab_thisfiber(gab), m, r, b) == gab_undefined)
    return gab_fpanic(gab, "$ already specializes for type $", m, r);

  return nullptr;
}

a_gab_value *gab_lib_def(struct gab_triple gab, size_t argc,
                         gab_value argv[static argc]) {
  gab_value m = gab_arg(0);
  gab_value b = gab_arg(argc - 1);

  if (gab_valkind(m) != kGAB_MESSAGE)
    return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

  size_t len = argc - 2;

  if (len == 0) {
    gab_value t = gab_undefined;

    if (gab_fibmsgput(gab, gab_thisfiber(gab), m, t, b) == gab_undefined)
      return gab_fpanic(gab, "$ already specializes for type $", m, t);

    return nullptr;
  }

  for (size_t i = 0; i < len; i++) {
    gab_value t = gab_arg(i + 1);

    if (gab_fibmsgput(gab, gab_thisfiber(gab), m, t, b) == gab_undefined)
      return gab_fpanic(gab, "$ already specializes for type $", m, t);
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
    gab_value b = gab_uvrecat(cases, i);
    gab_value t = gab_ukrecat(cases, i);

    if (gab_fibmsgput(gab, gab_thisfiber(gab), m, t, b) == gab_undefined)
      return gab_fpanic(gab, "$ already specializes for type $", m, t);
  }

  return nullptr;
}

a_gab_value *gab_lib_module(struct gab_triple gab, size_t argc,
                            gab_value argv[static argc]) {
  gab_value cases = gab_arg(0);
  gab_value messages = gab_arg(1);

  if (gab_valkind(cases) != kGAB_RECORD)
    return gab_pktypemismatch(gab, cases, kGAB_RECORD);

  if (gab_valkind(messages) != kGAB_RECORD)
    return gab_pktypemismatch(gab, messages, kGAB_RECORD);

  if (gab_reclen(cases) == 0) {
    gab_value t = gab_undefined;

    for (size_t i = 0; i < gab_reclen(messages); i++) {
      gab_value b = gab_uvrecat(messages, i);
      gab_value m = gab_ukrecat(messages, i);

      if (gab_valkind(m) != kGAB_MESSAGE)
        return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

      if (gab_fibmsgput(gab, gab_thisfiber(gab), m, t, b) == gab_undefined)
        return gab_fpanic(gab, "$ already specializes for type $", m, t);
    }

    return nullptr;
  }

  for (int j = 0; j < gab_reclen(cases); j++) {
    gab_value t = gab_uvrecat(cases, j);

    for (int i = 0; i < gab_reclen(messages); i++) {
      gab_value b = gab_uvrecat(messages, i);
      gab_value m = gab_ukrecat(messages, i);

      if (gab_valkind(m) != kGAB_MESSAGE)
        return gab_pktypemismatch(gab, m, kGAB_MESSAGE);

      if (gab_fibmsgput(gab, gab_thisfiber(gab), m, t, b) == gab_undefined)
        return gab_fpanic(gab, "$ already specializes for type $", m, t);
    }
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_type(gab, kGAB_MESSAGE);

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_strtosig(type),
          gab_snative(gab, "message.new", gab_lib_message),
      },
      {
          "impls",
          gab_strtosig(type),
          gab_snative(gab, "message.impls", gab_lib_impls),
      },
      {
          "sigils.into",
          type,
          gab_snative(gab, "sigils.into", gab_lib_sigil_into),
      },
      {
          "strings.into",
          type,
          gab_snative(gab, "strings.into", gab_lib_string_into),
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
