#include "include/gab.h"
#include <assert.h>
#include <stdio.h>

void dis_block(gab_value blk) {
  printf("     %V\n", blk);
  gab_fdis(stdout, GAB_VAL_TO_BLOCK_PROTO(GAB_VAL_TO_BLOCK(blk)->p)->mod);
}

void dis_message(struct gab_eg *gab, struct gab_vm *vm, gab_value msg,
                 gab_value rec) {
  gab_value spec = gab_msgat(msg, rec);

  switch (gab_valknd(spec)) {
  case kGAB_BLOCK:
    dis_block(spec);
    break;
  case kGAB_BUILTIN:
    printf("%V\n", spec);
    break;
  default:
    gab_panic(gab, vm, "Invalid message");
  }
}

void gab_lib_disstring(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                       size_t argc, gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
    return;
  }

  gab_value msg = gab_message(gab, argv[0]);

  dis_message(gab, vm, msg, argc == 1 ? gab_undefined : argv[1]);
};

void gab_lib_dismessage(struct gab_eg *gab, struct gab_gc *gc,
                        struct gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[0]) != kGAB_MESSAGE) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
    return;
  }

  dis_message(gab, vm, argv[0], gab_valtyp(gab, argv[1]));
}

void gab_lib_disblock(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                      size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  dis_block(argv[0]);
}

void gab_lib_disbuiltin(struct gab_eg *gab, struct gab_gc *gc,
                        struct gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
    return;
  }

  printf("%V\n", argv[0]);
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  gab_value receivers[] = {
      gab_typ(gab, kGAB_BLOCK),
      gab_typ(gab, kGAB_MESSAGE),
      gab_typ(gab, kGAB_STRING),
      gab_typ(gab, kGAB_BUILTIN),
  };

  gab_value values[] = {gab_sbuiltin(gab, "disblock", gab_lib_disblock),
                        gab_sbuiltin(gab, "dismessage", gab_lib_dismessage),
                        gab_sbuiltin(gab, "disstring", gab_lib_disstring),
                        gab_sbuiltin(gab, "disbuiltin", gab_lib_disbuiltin)};

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));

  for (uint8_t i = 0; i < LEN_CARRAY(values); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = "dis",
                      .receiver = receivers[i],
                      .specialization = values[i],
                  });
  }

  return a_gab_value_one(gab_nil);
}
