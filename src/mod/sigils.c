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
