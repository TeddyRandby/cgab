#include "include/gab.h"
#include <stdio.h>
#include <string.h>

void gab_lib_len(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
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

void gab_lib_slice(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  char *str = gab_valtocs(gab, argv[0]);

  u64 len = gab_strlen(argv[0]);
  u64 start = 0, end = len;

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

  u64 size = end - start;

  gab_value res = gab_nstring(gab, size, str + start);
  free(str);

  gab_vmpush(vm, res);
}

void gab_lib_split(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_STRING) {
    gab_panic(gab, vm, "&:split expects 2 arguments");
    return;
  }

  char *src = gab_valtocs(gab, argv[0]);
  char *delim_src = gab_valtocs(gab, argv[1]);

  size_t srclen = gab_strlen(argv[0]);
  size_t delimlen = gab_strlen(argv[1]);

  s_i8 delim = s_i8_cstr(delim_src);
  s_i8 window = s_i8_create((const i8 *)src, delimlen);

  const i8 *start = window.data;
  u64 len = 0;

  v_u64 splits;
  v_u64_create(&splits, 8);

  while (window.data + window.len <= (i8 *)src + srclen) {
    if (s_i8_match(window, delim)) {
      s_i8 split = s_i8_create(start, len);

      v_u64_push(&splits, gab_nstring(gab, split.len, (char *)split.data));

      window.data += window.len;
      start = window.data;
      len = 0;
    } else {
      len++;
      window.data++;
    }
  }

  s_i8 split = s_i8_create(start, len);
  v_u64_push(&splits, gab_nstring(gab, split.len, (char *)split.data));

  gab_value result = gab_tuple(gab, vm, splits.len, splits.data);

  gab_vmpush(vm, result);

  gab_gcdref(gab_vmgc(vm), vm, result);
}

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  gab_value string_type = gab_typ(gab, kGAB_STRING);

  const char *keys[] = {
      "slice",
      "split",
      "len",
  };

  gab_value values[] = {
      gab_builtin(gab, "slice", gab_lib_slice),
      gab_builtin(gab, "split", gab_lib_split),
      gab_builtin(gab, "len", gab_lib_len),
  };

  for (u8 i = 0; i < LEN_CARRAY(keys); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = keys[i],
                 .receiver = string_type,
                 .specialization = values[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(keys), values);

  return NULL;
}
