#include "include/gab.h"
#include <assert.h>
#include <stdio.h>

void dis_block(gab_value blk) {
  printf("     %V\n", blk);
  gab_fbcdump(stdout, GAB_VAL_TO_BLOCK_PROTO(GAB_VAL_TO_BLOCK(blk)->p));
}

void dis_message(struct gab_triple gab, gab_value msg, gab_value rec) {
  gab_value spec = gab_msgat(msg, rec);

  switch (gab_valkind(spec)) {
  case kGAB_BLOCK:
    dis_block(spec);
    break;
  case kGAB_NATIVE:
    printf("%V\n", spec);
    break;
  default:
    printf("%V\n", gab_nil);
    break;
  }
}

void gab_lib_disstring(struct gab_triple gab, size_t argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, "Invalid call to gab_lib_dis");
    return;
  }

  gab_value msg = gab_message(gab, argv[0]);

  dis_message(gab, msg, argc == 1 ? gab_undefined : argv[1]);
};

void gab_lib_dismessage(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  if (gab_valkind(argv[0]) != kGAB_MESSAGE) {
    gab_panic(gab, "Invalid call to gab_lib_dis");
    return;
  }

  switch (argc) {
  case 1:
    printf("%V\n", gab_msgrec(argv[0]));
    break;
  case 2:
    dis_message(gab, argv[0], argv[1]);
    break;
  default:
    gab_panic(gab, "Invalid call to gab_lib_dis");
  }
}

void gab_lib_disblock(struct gab_triple gab, size_t argc,
                      gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, "Invalid call to gab_lib_dis");

    return;
  }

  dis_block(argv[0]);
}

void gab_lib_disnative(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, "Invalid call to gab_lib_dis");
    return;
  }

  printf("%V\n", argv[0]);
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value receivers[] = {
      gab_type(gab.eg, kGAB_BLOCK),
      gab_type(gab.eg, kGAB_MESSAGE),
      gab_type(gab.eg, kGAB_STRING),
      gab_type(gab.eg, kGAB_NATIVE),
  };

  gab_value values[] = {gab_snative(gab, "disblock", gab_lib_disblock),
                        gab_snative(gab, "dismessage", gab_lib_dismessage),
                        gab_snative(gab, "disstring", gab_lib_disstring),
                        gab_snative(gab, "disnative", gab_lib_disnative)};

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
