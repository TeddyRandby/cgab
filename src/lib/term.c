#include "../include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <curses.h>
#include <ncurses.h>

void ncurses_cb(void *data) { endwin(); }

void gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {

  case 1: {
    initscr();

    gab_value k = GAB_CONTAINER(GAB_STRING("Term"), ncurses_cb, NULL, NULL);

    gab_push(vm, 1, &k);

    gab_val_dref(vm, k);

    break;
  }

  case 2: {
    if (!GAB_VAL_IS_RECORD(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    gab_obj_record *opts = GAB_VAL_TO_RECORD(argv[1]);

    initscr();

    if (gab_obj_record_has(opts, GAB_STRING("noecho")))
      noecho();

    if (gab_obj_record_has(opts, GAB_STRING("raw")))
      raw();

    if (gab_obj_record_has(opts, GAB_STRING("keypad")))
      keypad(stdscr, true);

    gab_value k = GAB_CONTAINER(GAB_STRING("Term"), ncurses_cb, NULL, NULL);

    gab_push(vm, 1, &k);

    gab_val_dref(vm, k);

    break;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
}

void gab_lib_refresh(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
  refresh();
}

void gab_lib_dim(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
    gab_value dim[] = { GAB_VAL_NUMBER(LINES), GAB_VAL_NUMBER(COLS)};

    gab_push(vm, 2, dim);
}

void gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (!GAB_VAL_IS_NUMBER(argv[1] || !GAB_VAL_IS_NUMBER(argv[2]) ||
                         !GAB_VAL_IS_STRING(argv[3]))) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }
  gab_obj_string* c = GAB_VAL_TO_STRING(argv[3]);

  if (c->len > 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  i32 x = GAB_VAL_TO_NUMBER(argv[1]);
  i32 y = GAB_VAL_TO_NUMBER(argv[2]);

  mvaddch(y, x, c->data[0]);
}

void gab_lib_print(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
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
      GAB_STRING("Term"),
      GAB_STRING("Term"),
  };

  gab_value names[] = {
      GAB_STRING("new"),
      GAB_STRING("refresh"),
      GAB_STRING("key"),
      GAB_STRING("print"),
      GAB_STRING("put"),
      GAB_STRING("dim"),
  };

  gab_value values[] = {
      GAB_BUILTIN(new),
      GAB_BUILTIN(refresh),
      GAB_BUILTIN(key),
      GAB_BUILTIN(print),
      GAB_BUILTIN(put),
      GAB_BUILTIN(dim),
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
