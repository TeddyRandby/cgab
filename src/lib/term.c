#include "../include/gab.h"
#include "include/object.h"
#include <ncurses.h>

void ncurses_cb(void *data) { endwin(); }

void gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc > 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_new");

    return;
  }

  initscr();

  gab_value k = GAB_CONTAINER(GAB_STRING("Term"), ncurses_cb, NULL);

  gab_push(vm, 1, &k);

  gab_val_dref(vm, k);
}

void gab_lib_refresh(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
  refresh();
}

void gab_lib_print(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
    for (u8 i = 1; i < argc; i++) {
        printw("%V", argv[i]);
    }
};

void gab_lib_key(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  i8 c = getch();

  gab_value res = GAB_VAL_OBJ(gab_obj_string_create(gab, s_i8_create(&c, 1)));

  gab_push(vm, 1, &res);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value term = GAB_SYMBOL("Term");

  gab_value receivers[] = {
      term,
      GAB_STRING("Term"),
      GAB_STRING("Term"),
      GAB_STRING("Term"),
  };

  gab_value names[] = {
      GAB_STRING("new"),
      GAB_STRING("refresh"),
      GAB_STRING("key"),
      GAB_STRING("print"),
  };

  gab_value values[] = {
      GAB_BUILTIN(new),
      GAB_BUILTIN(refresh),
      GAB_BUILTIN(key),
      GAB_BUILTIN(print),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (u8 i = 0; i < LEN_CARRAY(values); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  gab_val_dref(vm, term);
  return term;
}
