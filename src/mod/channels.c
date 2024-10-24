#include "gab.h"

a_gab_value *gab_chnlib_close(struct gab_triple gab, uint64_t argc,
                           gab_value argv[argc]) {
  gab_chnclose(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_arg(0));

  return nullptr;
}

a_gab_value *gab_chnlib_is_closed(struct gab_triple gab, uint64_t argc,
                              gab_value argv[argc]) {
  bool closed = gab_chnisclosed(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_bool(closed));

  return nullptr;
}

a_gab_value *gab_chnlib_is_full(struct gab_triple gab, uint64_t argc,
                            gab_value argv[argc]) {
  bool full = gab_chnisfull(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_bool(full));

  return nullptr;
}

a_gab_value *gab_chnlib_is_empty(struct gab_triple gab, uint64_t argc,
                             gab_value argv[argc]) {
  bool empty = gab_chnisempty(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_bool(empty));

  return nullptr;
}
