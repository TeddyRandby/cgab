#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <assert.h>
#include <stdio.h>

void dis_closure(gab_obj_block *cls) {
  printf("     %V\n", GAB_VAL_OBJ(cls));
  gab_dis(cls->p->mod);
}

void dis_message(gab_engine *gab, gab_obj_message *msg, gab_value rec) {
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

void gab_lib_disstring(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_obj_message *msg = gab_obj_message_create(gab, argv[0]);

  dis_message(gab, msg, argc == 1 ? GAB_VAL_UNDEFINED() : argv[1]);
};

void gab_lib_dismessage(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_obj_message *msg = GAB_VAL_TO_MESSAGE(argv[0]);

  dis_message(gab, msg, argv[1]);
}

void gab_lib_disclosure(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_obj_block *cls = GAB_VAL_TO_BLOCK(argv[0]);

  dis_closure(cls);
}

void gab_lib_disbuiltin(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");

    return;
  }

  gab_val_dump(stdout, argv[0]);
  printf("\n");
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_BLOCK),
      gab_type(gab, GAB_KIND_MESSAGE),
      gab_type(gab, GAB_KIND_STRING),
      gab_type(gab, GAB_KIND_BUILTIN),
  };

  gab_value values[] = {
      GAB_BUILTIN(disclosure),
      GAB_BUILTIN(dismessage),
      GAB_BUILTIN(disstring),
      GAB_BUILTIN(disbuiltin),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));

  for (u8 i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, vm, GAB_STRING("dis"), receivers[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  return GAB_VAL_NIL();
}
