#include "../include/gab.h"
#include "../include/os.h"
#include "core.h"
#include <stdio.h>

a_gab_value *gab_lib_prybreak(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  gab_value vargv[] = {
      gab_box(gab,
              (struct gab_box_argt){
                  .data = &gab.vm,
                  .size = sizeof(struct gab_vm *),
                  .type = gab_string(gab, "gab.vm"),
              }),
  };
  const char *sargv[] = {"vm"};

  gab_repl(gab, (struct gab_repl_argt){
                    .name = "pry",
                    .result_prefix = "=> ",
                    .prompt_prefix = "pry> ",
                    .flags = fGAB_USE | fGAB_EXIT_ON_PANIC,
                    .len = 1,
                    .argv = vargv,
                    .sargv = sargv,
                });

  return nullptr;
}

a_gab_value *gab_lib_pryframes(struct gab_triple gab, size_t argc,
                               gab_value argv[argc]) {
  if (argc < 1) {
    return gab_panic(gab, "Invalid call to gab_lib_pryframes");
  }

  if (argc == 1) {
    struct gab_triple with_vm = gab;
    with_vm.vm = *(struct gab_vm **)gab_boxdata(argv[0]);
    gab_value frame = gab_vmframe(with_vm, 0);

    gab_vmpush(gab.vm, frame);
    return nullptr;
  }

  if (argc == 2 && gab_valkind(argv[1]) == kGAB_NUMBER) {
    uint64_t depth = gab_valton(argv[1]);

    struct gab_triple with_vm = gab;
    with_vm.vm = *(struct gab_vm **)gab_boxdata(argv[0]);
    gab_value frame = gab_vmframe(with_vm, depth);

    gab_vmpush(gab.vm, frame);
    return nullptr;
  }

  return nullptr;
}

a_gab_value *gab_lib_prydumpframe(struct gab_triple gab, size_t argc,
                                  gab_value argv[argc]) {
  if (argc < 1)
    return gab_panic(gab, "Invalid call to gab_lib_pryframes");

  if (argc == 1) {
    struct gab_vm *vm = *(struct gab_vm **)gab_boxdata(argv[0]);
    gab_fvminspect(stdout, vm, 0);
    return nullptr;
  }

  if (argc == 2 && gab_valkind(argv[1]) == kGAB_NUMBER) {
    uint64_t depth = gab_valton(argv[1]);
    struct gab_vm *vm = *(struct gab_vm **)gab_boxdata(argv[0]);
    gab_fvminspect(stdout, vm, depth);
    return nullptr;
  }

  return nullptr;
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

  return nullptr;
}
