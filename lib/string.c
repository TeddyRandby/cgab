#include "include/gab.h"
#include <stdio.h>
#include <string.h>

void gab_lib_len(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, "&:len expects 1 argument");
    return;
  }

  gab_value result = gab_number(gab_strlen(argv[0]));

  gab_vmpush(gab.vm, result);
};

void gab_lib_new(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  switch (argc) {
    default:
      gab_panic(gab, "&:string.new expects 1 argument");
      return;
  }
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (MAX(0, MIN(a, b)))

void gab_lib_slice(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  s_char str = gab_valintocs(gab, argv[0]);

  uint64_t len = gab_strlen(argv[0]);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 2:
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, "&:slice expects a number as the second argument");
      return;
    }

    double a = gab_valton(argv[1]);
    end = MIN(a, len);
    break;

  case 3:
    if (gab_valknd(argv[1]) == kGAB_NUMBER) {
      start = MIN(gab_valton(argv[1]), len);
    } else if (argv[1] == gab_nil) {
      gab_panic(gab, "&:slice expects a number as the second argument");
      return;
    }

    if (gab_valknd(argv[2]) == kGAB_NUMBER) {
      end = MIN(gab_valton(argv[2]), len);
    } else if (argv[2] == gab_nil) {
      gab_panic(gab, "&:slice expects a number as the third argument");
      return;
    }
    break;

  default:
    gab_panic(gab, "&:slice expects 2 or 3 arguments");
    return;
  }

  if (start >= end) {
    gab_panic(gab, "&:slice expects the start to be before the end");
    return;
  }

  uint64_t size = end - start;

  gab_value res = gab_nstring(gab, size, str.data + start);

  gab_vmpush(gab.vm, res);
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value string_type = gab_typ(gab.eg, kGAB_STRING);

  gab_value receivers[] = {
      string_type,
      string_type,
      string_type,
  };

  const char *names[] = {
      "slice",
      "len",
      "string.new",
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "slice", gab_lib_slice),
      gab_sbuiltin(gab, "len", gab_lib_len),
      gab_sbuiltin(gab, "string.new", gab_lib_new),
  };

  for (uint8_t i = 0; i < LEN_CARRAY(names); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = specs[i],
                  });
  }

  return NULL;
}
