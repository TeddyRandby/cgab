#include "gab.h"

a_gab_value *gab_lib_now(struct gab_triple gab, uint64_t argc,
                         gab_value argv[argc]) {
  if (argc != 1) {
    return gab_fpanic(gab, "Invalid call to gab_lib_clock");
  }

  clock_t t = clock();

  gab_value res = gab_number((double)t / CLOCKS_PER_SEC);

  gab_vmpush(gab_vm(gab), res);
  return nullptr;
};
