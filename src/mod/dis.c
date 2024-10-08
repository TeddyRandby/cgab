#include "gab.h"
#include <stdio.h>

void dis_block(gab_value blk) {
  printf("     ");
  gab_fvalinspect(stdout, blk, 0);
  printf("\n");
  gab_fmodinspect(stdout, GAB_VAL_TO_PROTOTYPE(GAB_VAL_TO_BLOCK(blk)->p));
}

void dis_message(struct gab_triple gab, gab_value msg, gab_value rec) {
  gab_value spec = gab_fibmsgat(gab_thisfiber(gab), msg, rec);

  switch (gab_valkind(spec)) {
  case kGAB_BLOCK:
    dis_block(spec);
    break;
  case kGAB_NATIVE:
    gab_fvalinspect(stdout, spec, 0);
    printf("\n");
    break;
  default:
    gab_fvalinspect(stdout, gab_nil, 0);
    printf("\n");
    break;
  }
}

a_gab_value* gab_lib_disstring(struct gab_triple gab, size_t argc,
                      gab_value argv[argc]) {
  if (argc < 1)
    return gab_fpanic(gab, "Invalid call to gab_lib_dis");

  gab_value msg = gab_strtomsg(argv[0]);

  dis_message(gab, msg, argc == 1 ? gab_undefined : argv[1]);

  return nullptr;
};

a_gab_value* gab_lib_dismessage(struct gab_triple gab, size_t argc,
                       gab_value argv[argc]) {
  if (gab_valkind(argv[0]) != kGAB_MESSAGE)
    return gab_fpanic(gab, "Invalid call to gab_lib_dis");

  switch (argc) {
  case 1:
    dis_message(gab, argv[0], gab_undefined);
    break;
  case 2:
    dis_message(gab, argv[0], argv[1]);
    break;
  default:
    gab_fpanic(gab, "Invalid call to gab_lib_dis");
  }

  return nullptr;
}

a_gab_value* gab_lib_disblock(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    return gab_fpanic(gab, "Invalid call to gab_lib_dis");
  }

  dis_block(argv[0]);

  return nullptr;
}

a_gab_value* gab_lib_disnative(struct gab_triple gab, size_t argc,
                      gab_value argv[argc]) {
  if (argc != 1)
    return gab_fpanic(gab, "Invalid call to gab_lib_dis");

  gab_fvalinspect(stdout, argv[0], 0);
  printf("\n");

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value receivers[] = {
      gab_type(gab, kGAB_BLOCK),
      gab_type(gab, kGAB_MESSAGE),
      gab_type(gab, kGAB_STRING),
      gab_type(gab, kGAB_NATIVE),
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

  return nullptr;
}
