#include "include/gab.h"
#include <assert.h>
#include <stdio.h>

void dis_closure(gab_value cls) {
  printf("     %V\n", cls);
  gab_fdis(stdout, cls->p->mod);
}

void dis_message(gab_eg *gab, gab_obj_message *msg, gab_value rec) {
  gab_value spec = gab_obj_message_read(msg, gab_val_type(gab, rec));

  if (GAB_VAL_IS_BLOCK(spec)) {
    gab_obj_block *cls = GAB_VAL_TO_BLOCK(spec);
    dis_closure(cls);

    return;
  }

  if (GAB_VAL_IS_BUILTIN(spec)) {
    gab_val_dump(stdout, spec);
    printf("\n");
  }
}

void gab_lib_disstring(gab_eg *gab, gab_vm *vm, size_t argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_obj_message *msg = gab_obj_message_create(gab, argv[0]);

  dis_message(gab, msg, argc == 1 ? gab_undefined : argv[1]);
};

void gab_lib_dismessage(gab_eg *gab, gab_vm *vm, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_obj_message *msg = GAB_VAL_TO_MESSAGE(argv[0]);

  dis_message(gab, msg, argv[1]);
}

void gab_lib_disclosure(gab_eg *gab, gab_vm *vm, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_obj_block *cls = GAB_VAL_TO_BLOCK(argv[0]);

  dis_closure(cls);
}

void gab_lib_disbuiltin(gab_eg *gab, gab_vm *vm, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_val_dump(stdout, argv[0]);
  printf("\n");
}

gab_value gab_mod(gab_eg *gab, gab_vm *vm) {
  gab_value receivers[] = {
      gab_typ(gab, kGAB_BLOCK),
      gab_typ(gab, kGAB_MESSAGE),
      gab_typ(gab, kGAB_STRING),
      gab_typ(gab, kGAB_BUILTIN),
  };

  gab_value values[] = {
      GAB_BUILTIN(disclosure),
      GAB_BUILTIN(dismessage),
      GAB_BUILTIN(disstring),
      GAB_BUILTIN(disbuiltin),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));

  for (uint8_t i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, vm, gab_string(gab, "dis"), receivers[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  return gab_nil;
}
