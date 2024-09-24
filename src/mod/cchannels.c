#include "gab.h"

a_gab_value *gab_lib_close(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  gab_chnclose(gab_arg(0));

  return nullptr;
}

a_gab_value *gab_lib_isclosed(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  bool closed = gab_chnisclosed(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_bool(closed));

  return nullptr;
}

a_gab_value *gab_lib_isfull(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  bool full = gab_chnisfull(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_bool(full));

  return nullptr;
}

a_gab_value *gab_lib_isempty(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  bool empty = gab_chnisempty(gab_arg(0));

  gab_vmpush(gab_vm(gab), gab_bool(empty));

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value chan_type = gab_type(gab, kGAB_CHANNEL);

  gab_def(gab,
          {
              gab_message(gab, "close!"),
              chan_type,
              gab_snative(gab, "close!", gab_lib_close),
          },
          {
              gab_message(gab, "full?"),
              chan_type,
              gab_snative(gab, "close?", gab_lib_isfull),
          },
          {
              gab_message(gab, "empty?"),
              chan_type,
              gab_snative(gab, "empty?", gab_lib_isempty),
          },
          {
              gab_message(gab, "closed?"),
              chan_type,
              gab_snative(gab, "closed?", gab_lib_isclosed),
          });

  return a_gab_value_one(chan_type);
}
