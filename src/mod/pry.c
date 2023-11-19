#include "../include/gab.h"
#include "../include/os.h"
#include "core.h"
#include <stdio.h>

void gab_lib_prybreak(struct gab_triple gab, size_t argc,
                      gab_value argv[argc]) {
  gab_value vargv[] = {
      gab_box(gab,
              (struct gab_box_argt){
                  .data = gab.vm,
                  .type = gab_string(gab, "pry.vm"),
              }),
  };
  const char *sargv[] = {"vm"};

  gab_repl(gab, (struct gab_repl_argt){
                    .name = "pry",
                    .result_prefix = "=> ",
                    .prompt_prefix = "pry> ",
                    .flags = fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC,
                    .len = 1,
                    .argv = vargv,
                    .sargv = sargv,
                });
}

void gab_lib_pryframes(struct gab_triple gab, size_t argc,
                       gab_value argv[argc]) {
  if (argc < 1) {
    gab_panic(gab, "Invalid call to gab_lib_pryframes");
    return;
  }

  if (argc == 1) {
    gab_value frame = gab_vmframe(
        (struct gab_triple){
            .vm = gab_boxdata(argv[0]), .eg = gab.eg, .gc = gab.gc},
        0);

    gab_vmpush(gab.vm, frame);
    return;
  }

  if (argc == 2 && gab_valkind(argv[1]) == kGAB_NUMBER) {
    uint64_t depth = gab_valton(argv[1]);

    gab_value frame = gab_vmframe(
        (struct gab_triple){
            .vm = gab_boxdata(argv[0]), .eg = gab.eg, .gc = gab.gc},
        depth);

    gab_vmpush(gab.vm, frame);
    return;
  }
}

void gab_lib_prydumpframe(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  {
    if (argc < 1) {
      gab_panic(gab, "Invalid call to gab_lib_pryframes");
      return;
    }

    if (argc == 1) {
      gab_fvmdump(stdout, gab_boxdata(argv[0]), 0);
      return;
    }

    if (argc == 2 && gab_valkind(argv[1]) == kGAB_NUMBER) {
      uint64_t depth = gab_valton(argv[1]);

      gab_fvmdump(stdout, gab_boxdata(argv[0]), depth);
      return;
    }
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "pry.break",
          gab_undefined,
          gab_snative(gab, "pry.break", gab_lib_prybreak),
      },
      {
          "pry.frame",
          gab_string(gab, "gab.vm"),
          gab_snative(gab, "pry.frame", gab_lib_pryframes),
      },
      {
          "pry.dump",
          gab_string(gab, "gab.vm"),
          gab_snative(gab, "pry.dump", gab_lib_prydumpframe),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return a_gab_value_one(gab_nil);
}
