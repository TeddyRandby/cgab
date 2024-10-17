#include "gab.h"

a_gab_value *gab_lib_string_into(struct gab_triple gab, uint64_t argc,
                                 gab_value argv[static argc]) {
  gab_vmpush(gab_vm(gab), gab_sigtostr(gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib_messages_into(struct gab_triple gab, uint64_t argc,
                                   gab_value argv[static argc]) {
  gab_vmpush(gab_vm(gab), gab_sigtomsg(gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_type(gab, kGAB_SIGIL);

  gab_def(gab,
          {
              gab_message(gab, "strings.into"),
              type,
              gab_snative(gab, "strings.into", gab_lib_string_into),
          },
          {
              gab_message(gab, "messages.into"),
              type,
              gab_snative(gab, "messages.into", gab_lib_messages_into),
          });

  return a_gab_value_one(type);
}
