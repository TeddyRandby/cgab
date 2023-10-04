#include "../include/gab.h"
#include <curses.h>
#include <ncurses.h>

void ncurses_cb(void *data) { endwin(); }

void gab_lib_new(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  switch (argc) {

  case 1: {
    initscr();

    gab_value k = gab_box(
        gab, (struct gab_box_argt){
                 .data = NULL,
                 .type = gab_gciref(gab, gc, vm, gab_string(gab, "Term")),
                 .destructor = ncurses_cb,
                 .visitor = NULL,
             });

    gab_vmpush(vm, k);

    gab_gcdref(gab, gc, vm, k);

    break;
  }

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_RECORD) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    initscr();

    if (gab_srechas(gab, argv[1], "noecho"))
      noecho();

    if (gab_srechas(gab, argv[1], "raw"))
      raw();

    if (gab_srechas(gab, argv[1], "keypad"))
      keypad(stdscr, true);

    gab_value k = gab_box(
        gab, (struct gab_box_argt){
                 .data = NULL,
                 .type = gab_gciref(gab, gc, vm, gab_string(gab, "Term")),
                 .destructor = ncurses_cb,
                 .visitor = NULL,
             });

    gab_vmpush(vm, k);

    gab_gcdref(gab, gc, vm, k);

    break;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
}

void gab_lib_refresh(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                     size_t argc, gab_value argv[argc]) {
  refresh();
}

void gab_lib_dim(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  gab_value dim[] = {gab_number(LINES), gab_number(COLS)};

  gab_nvmpush(vm, 2, dim);
}

void gab_lib_put(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (gab_valknd(argv[1]) != kGAB_NUMBER ||
      gab_valknd(argv[2]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  s_char str = gab_valintocs(gab, argv[3]);

  if (str.len != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  int32_t x = gab_valton(argv[1]);
  int32_t y = gab_valton(argv[2]);

  mvaddch(y, x, str.data[0]);
}

void gab_lib_print(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  for (uint8_t i = 1; i < argc; i++) {
    printw("%V", argv[i]);
  }
};

void gab_lib_key(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  int8_t c = getch();

  gab_value res = gab_nstring(gab, 1, (char *)&c);

  gab_vmpush(vm, res);
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  gab_value receivers[] = {
      gab_nil,
      gab_string(gab, "Term"),
      gab_string(gab, "Term"),
      gab_string(gab, "Term"),
      gab_string(gab, "Term"),
      gab_string(gab, "Term"),
  };

  const char *names[] = {
      "term", "refresh", "key", "print", "put", "dim",
  };

  gab_value values[] = {
      gab_sbuiltin(gab, "new", gab_lib_new),
      gab_sbuiltin(gab, "refresh", gab_lib_refresh),
      gab_sbuiltin(gab, "key", gab_lib_key),
      gab_sbuiltin(gab, "print", gab_lib_print),
      gab_sbuiltin(gab, "put", gab_lib_put),
      gab_sbuiltin(gab, "dim", gab_lib_dim),
  };

  static_assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (uint8_t i = 0; i < LEN_CARRAY(values); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .receiver = receivers[i],
                      .name = names[i],
                      .specialization = values[i],
                  });
  }

  gab_ngciref(gab, gc, vm, 1, LEN_CARRAY(receivers), receivers);

  return NULL;
}
