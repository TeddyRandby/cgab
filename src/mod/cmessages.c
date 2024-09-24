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
    gab_value rec = GAB_VAL_TO_FIBER(gab_thisfiber(gab))->messages;
    gab_vmpush(gab_vm(gab), rec);

    return nullptr;
  }

  gab_value msg = gab_arg(1);

  gab_value rec = GAB_VAL_TO_FIBER(gab_thisfiber(gab))->messages;
  rec = rec == gab_undefined ? rec : gab_recat(rec, msg);

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
  gab_value msg = gab_arg(0);
  gab_value rec = gab_arg(1);
  gab_value spec = gab_arg(2);

  if (gab_valkind(spec) != kGAB_PRIMITIVE && gab_valkind(spec) != kGAB_BLOCK &&
      gab_valkind(spec) != kGAB_NATIVE)
    return gab_pktypemismatch(gab, spec, kGAB_BLOCK);

  if (!gab_def(gab, {msg, rec, spec}))
    return gab_fpanic(gab, "$ already specializes for type $", msg, rec);

  return nullptr;
}

a_gab_value *gab_lib_def(struct gab_triple gab, size_t argc,
                         gab_value argv[static argc]) {
  gab_value msg = gab_arg(0);
  gab_value spec = gab_arg(argc - 1);

  if (gab_valkind(msg) != kGAB_MESSAGE)
    return gab_pktypemismatch(gab, msg, kGAB_MESSAGE);

  size_t len = argc - 2;

  if (len == 0) {
    gab_value t = gab_undefined;

    if (!gab_def(gab, {msg, t, spec}))
      return gab_fpanic(gab, "$ already specializes for type $", msg, t);

    return nullptr;
  }

  for (size_t i = 0; i < len; i++) {
    gab_value t = gab_arg(i + 1);

    if (!gab_def(gab, {msg, t, spec}))
      return gab_fpanic(gab, "$ already specializes for type $", msg, t);
  }

  return nullptr;
}

a_gab_value *gab_lib_case(struct gab_triple gab, size_t argc,
                          gab_value argv[static argc]) {
  gab_value msg = gab_arg(0);
  gab_value cases = gab_arg(1);

  if (gab_valkind(msg) != kGAB_MESSAGE)
    return gab_pktypemismatch(gab, msg, kGAB_MESSAGE);

  if (gab_valkind(cases) != kGAB_RECORD)
    return gab_pktypemismatch(gab, cases, kGAB_RECORD);

  for (int i = 0; i < gab_reclen(cases); i++) {
    gab_value type = gab_ukrecat(cases, i);
    gab_value spec = gab_uvrecat(cases, i);

    if (!gab_def(gab, {msg, type, spec}))
      return gab_fpanic(gab, "$ already specializes for type $", msg, type);
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
    gab_value type = gab_undefined;

    for (size_t i = 0; i < gab_reclen(messages); i++) {
      gab_value spec = gab_uvrecat(messages, i);
      gab_value msg = gab_ukrecat(messages, i);

      if (gab_valkind(msg) != kGAB_MESSAGE)
        return gab_pktypemismatch(gab, msg, kGAB_MESSAGE);

      if (!gab_def(gab, {msg, type, spec}))
        return gab_fpanic(gab, "$ already specializes for type $", msg, type);
    }

    return nullptr;
  }

  for (int j = 0; j < gab_reclen(cases); j++) {
    gab_value type = gab_uvrecat(cases, j);

    for (int i = 0; i < gab_reclen(messages); i++) {
      gab_value spec = gab_uvrecat(messages, i);
      gab_value msg = gab_ukrecat(messages, i);

      if (gab_valkind(msg) != kGAB_MESSAGE)
        return gab_pktypemismatch(gab, msg, kGAB_MESSAGE);

      if (!gab_def(gab, {msg, type, spec}))
        return gab_fpanic(gab, "$ already specializes for type $", msg, type);
    }
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_type(gab, kGAB_MESSAGE);

  gab_def(gab,
          {
              gab_message(gab, mGAB_CALL),
              gab_strtosig(type),
              gab_snative(gab, "message.new", gab_lib_message),
          },
          {
              gab_message(gab, "impls"),
              gab_strtosig(type),
              gab_snative(gab, "message.impls", gab_lib_impls),
          },
          {
              gab_message(gab, "sigils.into"),
              type,
              gab_snative(gab, "sigils.into", gab_lib_sigil_into),
          },
          {
              gab_message(gab, "strings.into"),
              type,
              gab_snative(gab, "strings.into", gab_lib_string_into),
          },
          {
              gab_message(gab, "has?"),
              type,
              gab_snative(gab, "has?", gab_lib_has),
          },
          {
              gab_message(gab, "at"),
              type,
              gab_snative(gab, "at", gab_lib_at),
          },
          {
              gab_message(gab, "def!"),
              type,
              gab_snative(gab, "def!", gab_lib_def),
          },
          {
              gab_message(gab, "defcase!"),
              type,
              gab_snative(gab, "defcase!", gab_lib_case),
          },
          {
              gab_message(gab, "defmodule!"),
              gab_undefined,
              gab_snative(gab, "defmodule!", gab_lib_module),
          });

  return a_gab_value_one(type);
}
