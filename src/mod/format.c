#include "gab.h"
#include <stdio.h>

a_gab_value *fmt_panicf(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  gab_value fmt = gab_arg(0);

  if (gab_valkind(fmt) != kGAB_STRING)
    return gab_pktypemismatch(gab, fmt, kGAB_STRING);

  const char *cfmt = gab_strdata(&fmt);

  return gab_fpanic(gab, cfmt);
}

a_gab_value *fmt_printf(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  gab_value fmt = gab_arg(0);

  if (gab_valkind(fmt) != kGAB_STRING)
    return gab_pktypemismatch(gab, fmt, kGAB_STRING);

  const char *cfmt = gab_strdata(&fmt);

  gab_nfprintf(stdout, cfmt, argc - 1, argv + 1);

  return nullptr;
}

a_gab_value *fmt_print(struct gab_triple gab, size_t argc,
                       gab_value argv[argc]) {
  gab_fvalinspect(stdout, gab_arg(0), 2);

  for (size_t i = 1; i < argc; i++) {
    fprintf(stdout, ", ");
    gab_fvalinspect(stdout, gab_arg(i), 0);
  }

  fputc('\n', stdout);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "panic",
          gab_type(gab, kGAB_STRING),
          gab_snative(gab, "panic", fmt_panicf),
      },
      {
          "printf",
          gab_type(gab, kGAB_STRING),
          gab_snative(gab, "printf", fmt_printf),
      },
      {
          "print",
          gab_undefined,
          gab_snative(gab, "print", fmt_print),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return nullptr;
}
