#include "include/gab.h"
#include "list.h"
#include "map.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void gab_lib_send(gab_eg *gab, gab_vm *vm, size_t argc,
                  gab_value argv[static argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  a_gab_value *result = gab_send(gab, vm, argv[1], argv[0], argc - 2, argv + 2);

  if (!result) {
    gab_panic(gab, vm, "Invalid send");
    return;
  }

  gab_nvmpush(vm, result->len, result->data);

  a_gab_value_destroy(result);
}

void gab_lib_splat(gab_eg *gab, gab_vm *vm, size_t argc,
                   gab_value argv[static argc]) {
  gab_nvmpush(vm, gab_reclen(argv[0]), gab_recdata(argv[0]));
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))
void gab_lib_slice(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (gab_valknd(argv[0]) != kGAB_RECORD) {
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  gab_value rec = argv[0];

  u64 len = gab_reclen(rec);
  u64 start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 3: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER ||
        gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = gab_valton(argv[1]);
    start = MAX(a, 0);

    u64 b = gab_valton(argv[2]);
    end = MIN(b, len);
    break;
  }

  case 2:
    if (gab_valknd(argv[1]) == kGAB_NUMBER) {
      u64 a = gab_valton(argv[1]);
      end = MIN(a, len);
      break;
    }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  u64 result_len = end - start;

  gab_value result = gab_tuple(gab, vm, result_len, gab_recdata(rec) + start);

  gab_vmpush(vm, result);

  gab_gcdref(gab_vmgc(vm), vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

void gab_lib_at(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "Invalid call to  gab_lib_at");
    return;
  }

  gab_value result = gab_recat(argv[0], argv[1]);

  gab_vmpush(vm, result);
}

void gab_lib_put(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 3) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  gab_recput(vm, argv[0], argv[1], argv[2]);

  gab_vmpush(vm, argv[1]);
}

void gab_lib_next(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
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

void gab_lib_new(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_SHAPE) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    gab_value result = gab_erecord(gab, argv[1]);

    gab_vmpush(vm, result);

    gab_gcdref(gab_vmgc(vm), vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
};

void gab_lib_len(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
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

void gab_lib_to_l(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list =
        list_create(gab, vm, gab_reclen(argv[0]), gab_recdata(argv[0]));

    gab_vmpush(vm, list);

    gab_gcdref(gab_vmgc(vm), vm, list);

    return;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_to_l");
    return;
  }
}

void gab_lib_implements(gab_eg *gab, gab_vm *vm, size_t argc,
                        gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_MESSAGE) {
      gab_panic(gab, vm, "Invalid call to gab_lib_implements");
      return;
    }

    bool implements = gab_msgfind(argv[1], argv[0]) != UINT64_MAX;

    gab_value result = gab_bool(implements);

    gab_vmpush(vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_implements");
    return;
  }
}

void gab_lib_to_m(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  gab_value rec = argv[0];
  gab_value shp = gab_recshp(rec);

  switch (argc) {
  case 1: {
    gab_value map = map_create(gab, vm, gab_reclen(rec), 1, gab_shpdata(shp),
                               gab_recdata(rec));

    gab_vmpush(vm, map);

    gab_gcdref(gab_vmgc(vm), vm, map);

    return;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_to_m");
    return;
  }
}
a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "record", "len",  "to_l",  "to_m",  "send",        "put",
      "at",     "next", "slice", "splat", "implements?",
  };

  gab_value receivers[] = {
      gab_nil,
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_UNDEFINED),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_RECORD),
      gab_typ(gab, kGAB_UNDEFINED),
  };

  gab_value specs[] = {
      gab_builtin(gab, "new", gab_lib_new),
      gab_builtin(gab, "len", gab_lib_len),
      gab_builtin(gab, "to_l", gab_lib_to_l),
      gab_builtin(gab, "to_m", gab_lib_to_m),
      gab_builtin(gab, "send", gab_lib_send),
      gab_builtin(gab, "put", gab_lib_put),
      gab_builtin(gab, "at", gab_lib_at),
      gab_builtin(gab, "next", gab_lib_next),
      gab_builtin(gab, "slice", gab_lib_slice),
      gab_builtin(gab, "splat", gab_lib_splat),
      gab_builtin(gab, "implements?", gab_lib_implements),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = receivers[i],
                 .specialization = specs[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(specs), specs);

  return NULL;
}
