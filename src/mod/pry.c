#include "gab.h"

a_gab_value *gab_lib_prybreak(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  gab_value vargv[] = {gab_fb(gab)};
  const char *sargv[] = {"fiber"};

  gab_repl(gab, (struct gab_repl_argt){
                    .name = "pry",
                    .result_prefix = "=> ",
                    .prompt_prefix = "pry> ",
                    .flags = fGAB_ERR_EXIT,
                    .len = 1,
                    .argv = vargv,
                    .sargv = sargv,
                });

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
  gab_value pry_t = gab_sigil(gab, "pry");

  struct gab_spec_argt specs[] = {
      {
          "break",
          pry_t,
          gab_snative(gab, "pry.break", gab_lib_prybreak),
      },
      {
          "dump",
          gab_egtype(gab.eg, kGAB_FIBER),
          gab_snative(gab, "pry.dump", gab_lib_prydumpframe),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return a_gab_value_one(pry_t);
}
