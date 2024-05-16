#include "gab.h"
#include <stdio.h>

a_gab_value *fmt_printf(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  gab_value fmt = gab_arg(0);

  if (gab_valkind(fmt) != kGAB_STRING)
    return gab_pktypemismatch(gab, fmt, kGAB_STRING);

  const char *cfmt = gab_valintocs(gab, fmt);

  gab_afprintf(stdout, cfmt, argc - 1, argv + 1);

  return nullptr;
}

a_gab_value *fmt_print(struct gab_triple gab, size_t argc,
                       gab_value argv[argc]) {
  gab_fvalinspect(stdout, gab_arg(0), 2);

  return nullptr;
}

a_gab_value *fmt_println(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_fvalinspect(stdout, gab_arg(0), 2);
  fputc('\n', stdout);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "printf",
          gab_egtype(gab.eg, kGAB_STRING),
          gab_snative(gab, "write", fmt_printf),
      },
      {
          "print",
          gab_undefined,
          gab_snative(gab, "print", fmt_print),
      },
      {
          "println",
          gab_undefined,
          gab_snative(gab, "println", fmt_println),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return nullptr;
}
