#include "include/gab.h"
#include <stdio.h>
#include <string.h>

void gab_lib_len(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "&:len expects 1 argument");
    return;
  }

  gab_value result = gab_number(gab_strlen(argv[0]));

  gab_vmpush(vm, result);
};

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (MAX(0, MIN(a, b)))

void gab_lib_slice(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  s_char str = gab_valintocs(gab, argv[0]);

  uint64_t len = gab_strlen(argv[0]);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 2:
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "&:slice expects a number as the second argument");
      return;
    }

    double a = gab_valton(argv[1]);
    end = MIN(a, len);
    break;

  case 3:
    if (gab_valknd(argv[1]) == kGAB_NUMBER) {
      start = MIN(gab_valton(argv[1]), len);
    } else if (argv[1] == gab_nil) {
      gab_panic(gab, vm, "&:slice expects a number as the second argument");
      return;
    }

    if (gab_valknd(argv[2]) == kGAB_NUMBER) {
      end = MIN(gab_valton(argv[2]), len);
    } else if (argv[2] == gab_nil) {
      gab_panic(gab, vm, "&:slice expects a number as the third argument");
      return;
    }
    break;

  default:
    gab_panic(gab, vm, "&:slice expects 2 or 3 arguments");
    return;
  }

  if (start < end) {
    gab_panic(gab, vm, "&:slice expects the start to be before the end");
    return;
  }

  uint64_t size = end - start;

  gab_value res = gab_nstring(gab, size, str.data + start);

  gab_vmpush(vm, res);
}

void gab_lib_split(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_STRING) {
    gab_panic(gab, vm, "&:split expects 2 arguments");
    return;
  }

  s_char src = gab_valintocs(gab, argv[0]);
  s_char delim = gab_valintocs(gab, argv[1]);

  s_char window = s_char_create(src.data, delim.len);

  const char *start = window.data;
  uint64_t len = 0;

  v_uint64_t splits;
  v_uint64_t_create(&splits, 8);

  while (window.data + window.len <= src.data + src.len) {
    if (s_char_match(window, delim)) {
      s_char split = s_char_create(start, len);

      v_uint64_t_push(&splits, gab_nstring(gab, split.len, (char *)split.data));

      window.data += window.len;
      start = window.data;
      len = 0;
    } else {
      len++;
      window.data++;
    }
  }

  s_char split = s_char_create(start, len);
  v_uint64_t_push(&splits, gab_nstring(gab, split.len, (char *)split.data));

  gab_value result = gab_tuple(gab, splits.len, splits.data);

  gab_vmpush(vm, result);

  gab_gcdref(gab, gc, vm, result);
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  gab_value string_type = gab_typ(gab, kGAB_STRING);

  gab_value receivers[] = {
      string_type,
      string_type,
      string_type,
  };

  const char *names[] = {
      "slice",
      "split",
      "len",
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "slice", gab_lib_slice),
      gab_sbuiltin(gab, "split", gab_lib_split),
      gab_sbuiltin(gab, "len", gab_lib_len),
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
