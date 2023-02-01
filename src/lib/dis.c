#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <assert.h>
#include <stdio.h>

void dis_closure(gab_obj_closure *cls) {
  printf("     closure:\x1b[36m%.*s\x1b[0m\n", (int)cls->p->name.len,
         cls->p->name.data);

  gab_dis(cls->p->mod, cls->p->offset, cls->p->len);
}

void dis_message(gab_obj_message *msg, gab_value rec) {
  gab_value spec = gab_obj_message_read(msg, rec);

  if (GAB_VAL_IS_CLOSURE(spec)) {
    gab_obj_closure *cls = GAB_VAL_TO_CLOSURE(spec);
    dis_closure(cls);
  } else if (GAB_VAL_IS_BUILTIN(spec)) {
    gab_val_dump(spec);
  }
}

gab_value gab_lib_disstring(gab_engine *gab, gab_vm *vm, u8 argc,
                            gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
  }

  gab_obj_message *msg = gab_obj_message_create(
      gab, gab_obj_string_ref(GAB_VAL_TO_STRING(argv[0])));

  dis_message(msg, argc == 1 ? GAB_VAL_UNDEFINED() : argv[1]);

  return GAB_VAL_NIL();
};

gab_value gab_lib_dismessage(gab_engine *gab, gab_vm *vm, u8 argc,
                             gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
  }

  gab_obj_message *msg = GAB_VAL_TO_MESSAGE(argv[0]);

  dis_message(msg, argv[1]);

  return GAB_VAL_NIL();
}

gab_value gab_lib_disclosure(gab_engine *gab, gab_vm *vm, u8 argc,
                             gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
  }

  gab_obj_closure *cls = GAB_VAL_TO_CLOSURE(argv[0]);

  dis_closure(cls);

  return GAB_VAL_NIL();
}

gab_value gab_lib_disbuiltin(gab_engine *gab, gab_vm *vm, u8 argc,
                             gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_dis");
  }

  gab_val_dump(argv[0]);
  printf("\n");

  return GAB_VAL_NIL();
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value receivers[] = {
      gab_get_type(gab, TYPE_CLOSURE),
      gab_get_type(gab, TYPE_MESSAGE),
      gab_get_type(gab, TYPE_STRING),
      gab_get_type(gab, TYPE_BUILTIN),
  };

  gab_value values[] = {
      GAB_BUILTIN(disclosure),
      GAB_BUILTIN(dismessage),
      GAB_BUILTIN(disstring),
      GAB_BUILTIN(disbuiltin),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));

  for (u8 i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, s_i8_cstr("dis"), receivers[i], values[i]);
    gab_dref(gab, vm, values[i]);
  }

  return GAB_VAL_NIL();
}
