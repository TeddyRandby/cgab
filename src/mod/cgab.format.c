#include "gab.h"
#include <stdio.h>

a_gab_value *fmt_panicf(struct gab_triple gab, uint64_t argc,
                        gab_value argv[argc]) {
  gab_value fmt = gab_arg(0);

  if (gab_valkind(fmt) != kGAB_STRING)
    return gab_pktypemismatch(gab, fmt, kGAB_STRING);

  const char *cfmt = gab_strdata(&fmt);

  return gab_fpanic(gab, cfmt);
}

a_gab_value *fmt_printf(struct gab_triple gab, uint64_t argc,
                        gab_value argv[argc]) {
  gab_value fmt = gab_arg(0);

  if (gab_valkind(fmt) != kGAB_STRING)
    return gab_pktypemismatch(gab, fmt, kGAB_STRING);

  const char *cfmt = gab_strdata(&fmt);

  gab_nfprintf(gab.eg->sout, cfmt, argc - 1, argv + 1);

  return nullptr;
}

a_gab_value *fmt_print(struct gab_triple gab, uint64_t argc,
                       gab_value argv[argc]) {
  gab_fvalinspect(gab.eg->sout, gab_arg(0), 2);

  for (uint64_t i = 1; i < argc; i++) {
    fprintf(gab.eg->sout, ", ");
    gab_fvalinspect(gab.eg->sout, gab_arg(i), 0);
  }

  fputc('\n', gab.eg->sout);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_def(gab,
          {
              gab_message(gab, "panic"),
              gab_type(gab, kGAB_STRING),
              gab_snative(gab, "panic", fmt_panicf),
          },
          {
              gab_message(gab, "printf"),
              gab_type(gab, kGAB_STRING),
              gab_snative(gab, "printf", fmt_printf),
          },
          {
              gab_message(gab, "print"),
              gab_undefined,
              gab_snative(gab, "print", fmt_print),
          });

  return nullptr;
}
