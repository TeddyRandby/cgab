#include "include/gab.h"
#include "list.h"
#include "map.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void gab_lib_splat(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[static argc]) {
  gab_nvmpush(vm, gab_reclen(argv[0]), gab_recdata(argv[0]));
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))
void gab_lib_slice(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  if (gab_valknd(argv[0]) != kGAB_RECORD) {
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  gab_value rec = argv[0];

  uint64_t len = gab_reclen(rec);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 3: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER ||
        gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    uint64_t a = gab_valton(argv[1]);
    start = MAX(a, 0);

    uint64_t b = gab_valton(argv[2]);
    end = MIN(b, len);
    break;
  }

  case 2:
    if (gab_valknd(argv[1]) == kGAB_NUMBER) {
      uint64_t a = gab_valton(argv[1]);
      end = MIN(a, len);
      break;
    }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  uint64_t result_len = end - start;

  gab_value result = gab_tuple(gab, result_len, gab_recdata(rec) + start);

  gab_vmpush(vm, result);

  gab_gcdref(gab, gc, vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

void gab_lib_at(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                size_t argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to  gab_lib_at");
    return;
  }

  gab_value result = gab_recat(argv[0], argv[1]);

  gab_vmpush(vm, result);
}

void gab_lib_put(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (argc != 3) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  gab_recput(argv[0], argv[1], argv[2]);

  gab_vmpush(vm, argv[1]);
}

void gab_lib_next(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                  size_t argc, gab_value argv[argc]) {
  gab_value rec = argv[0];
  gab_value shp = gab_recshp(rec);

  gab_value res = gab_nil;

  size_t len = gab_reclen(rec);

  if (len < 1)
    goto fin;

  switch (argc) {
  case 1:
    res = gab_shpdata(shp)[0];
    goto fin;
  case 2: {
    size_t current = gab_shpfind(shp, argv[1]);

    if (current == UINT64_MAX || current + 1 == len)
      goto fin;

    res = gab_shpdata(shp)[current + 1];
    goto fin;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_next");
    return;
  }

fin:
  gab_vmpush(vm, res);
}
void gab_lib_tuple(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to :tuple");
      return;
    }

    gab_value result = gab_etuple(gab, gab_valton(argv[1]));

    gab_vmpush(vm, result);

    gab_gcdref(gab, gc, vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to :tuple");
    return;
  }
}

void gab_lib_record(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                    size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_SHAPE) {
      gab_panic(gab, vm, "Expected shape as second argument to :record");
      return;
    }

    gab_value result = gab_erecordof(gab, argv[1]);

    gab_vmpush(vm, result);

    gab_gcdref(gab, gc, vm, result);

    return;
  }
  default:
    for (int i = 0; i < argc; i++) {
      printf("%V\n", argv[i]);
    }
    gab_panic(gab, vm, "Expected 1 argument to :record");
    return;
  }
};

void gab_lib_len(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value result = gab_number(gab_reclen(argv[0]));

    gab_vmpush(vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }
}

void gab_lib_to_l(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                  size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list =
        list_create(gab, gc, vm, gab_reclen(argv[0]), gab_recdata(argv[0]));

    gab_vmpush(vm, list);

    gab_gcdref(gab, gc, vm, list);

    return;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_to_l");
    return;
  }
}

void gab_lib_to_m(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                  size_t argc, gab_value argv[argc]) {
  gab_value rec = argv[0];
  gab_value shp = gab_recshp(rec);

  switch (argc) {
  case 1: {
    gab_value map = map_create(gab, gc, vm, gab_reclen(rec), 1,
                               gab_shpdata(shp), gab_recdata(rec));

    gab_vmpush(vm, map);

    gab_gcdref(gab, gc, vm, map);

    return;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_to_m");
    return;
  }
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  const char *names[] = {
      "tuple", "record", "len",  "to_l",  "to_m",
      "put",   "at",     "next", "slice", "splat",
  };

  gab_value receivers[] = {
      gab_nil,
      gab_nil,
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "tuple", gab_lib_tuple),
      gab_sbuiltin(gab, "record", gab_lib_record),
      gab_sbuiltin(gab, "len", gab_lib_len),
      gab_sbuiltin(gab, "to_l", gab_lib_to_l),
      gab_sbuiltin(gab, "to_m", gab_lib_to_m),
      gab_sbuiltin(gab, "put", gab_lib_put),
      gab_sbuiltin(gab, "at", gab_lib_at),
      gab_sbuiltin(gab, "next", gab_lib_next),
      gab_sbuiltin(gab, "slice", gab_lib_slice),
      gab_sbuiltin(gab, "splat", gab_lib_splat),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = specs[i],
                  });
  }

  return NULL;
}
