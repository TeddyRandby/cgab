#include "core.h"
#include "gab.h"

a_gab_value *gab_lib_arrowfn(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  gab_value LHS = gab_arg(1);
  gab_value RHS = gab_arg(2);
  gab_value ENV = gab_arg(3);

  size_t len = gab_reclen(LHS);
  gab_value ids[len];
  for (size_t i = 0; i < len; i++) {
    gab_value id = gab_uvrecat(LHS, i);

    if (gab_valkind(id) != kGAB_MESSAGE)
      return gab_fpanic(gab, "Invalid parameter");

    ids[i] = id;
  }

  gab_value new_env = gab_shptorec(gab, gab_shape(gab, 1, len, ids));

  gab_value prototype = gab_unquote(gab, RHS, gab_lstpush(gab, ENV, new_env));

  if (prototype == gab_undefined)
    return gab_fpanic(gab, "Failed to unquote block");

  // TODO: Get modified ENV from unquote
  gab_value node = gab_recordof(
      gab, gab_message(gab, "gab.lhs"), gab_listof(gab, prototype),
      gab_message(gab, "gab.msg"), gab_message(gab, "gab.runtime.block"),
      gab_message(gab, "gab.rhs"), gab_listof(gab));

  gab_vmpush(gab_vm(gab), gab_listof(gab, node), ENV);

  return nullptr;
}
a_gab_value *gab_lib_assign(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {

  gab_value LHS = gab_arg(1);
  gab_value RHS = gab_arg(2);
  gab_value ENV = gab_arg(3);

  size_t local_env_idx = gab_reclen(ENV) - 1;
  gab_value local_env = gab_uvrecat(ENV, local_env_idx);

  // Go through all the messages in the left side
  // Look them up in the env, if they don't exist,
  // extend the local env.
  // emit a call to :gab.runtime/env.put!
  size_t len = gab_reclen(LHS);
  for (size_t i = 0; i < len; i++) {
    gab_value id = gab_uvrecat(LHS, i);

    if (gab_valkind(id) != kGAB_MESSAGE)
      return gab_fpanic(gab, "Invalid assignment target");

    local_env = gab_recput(gab, local_env, id, gab_nil);
  }

  // We reshape this node into a send for :gab.runtime/env.put!
  /*
   *[
   *  { gab.lhs: [ ...ids ],
   *    gab.msg: env.put!,
   *    gab.rhs: [ ...rhs ]
   *  }
   *]
   */

  gab_value node = gab_recordof(gab, gab_message(gab, "gab.lhs"), LHS,
                                gab_message(gab, "gab.msg"),
                                gab_message(gab, "gab.runtime.env.put!"),
                                gab_message(gab, "gab.rhs"), RHS);

  gab_vmpush(gab_vm(gab), gab_listof(gab, node),
             gab_urecput(gab, ENV, local_env_idx, local_env));

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_defmacro(gab,
               {
                   gab_message(gab, mGAB_ASSIGN),
                   gab_type(gab, kGAB_MESSAGE),
                   gab_snative(gab, "gab.assign", gab_lib_assign),
               },
               {
                   gab_message(gab, "=>"),
                   gab_type(gab, kGAB_MESSAGE),
                   gab_snative(gab, "gab/=>", gab_lib_arrowfn),
               });

  return a_gab_value_empty(0);
}
