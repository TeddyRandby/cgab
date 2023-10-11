#include "list.h"
#include "include/gab.h"
#include <assert.h>

void gab_lib_new(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list = list_create_empty(gab, gc, vm, 8);

    gab_vmpush(vm, list);

    gab_gcdref(gab, gc, vm, list);

    return;
  }
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    uint64_t len = gab_valton(argv[1]);

    gab_value list = list_create_empty(gab, gc, vm, len * 2);

    while (len--)
      v_gab_value_push(gab_boxdata(list), gab_nil);

    gab_vmpush(vm, list);

    gab_gcdref(gab, gc, vm, list);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
}

void gab_lib_len(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_value result = gab_number(((v_gab_value *)gab_boxdata(argv[0]))->len);

  gab_vmpush(vm, result);

  return;
}

void gab_lib_pop(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_value result = v_gab_value_pop(gab_boxdata(argv[0]));

  gab_vmpush(vm, result);

  return;
}

void gab_lib_push(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                  size_t argc, gab_value argv[argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  for (uint8_t i = 1; i < argc; i++)
    v_gab_value_push(gab_boxdata(argv[0]), argv[i]);

  gab_vmpush(vm, *argv);
}

void gab_lib_at(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  uint64_t offset = gab_valton(argv[1]);

  gab_value res = list_at(argv[0], offset);

  gab_vmpush(vm, res);
}

void gab_lib_del(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_del");
    return;
  }

  uint64_t index = gab_valton(argv[1]);

  v_gab_value_del(gab_boxdata(argv[0]), index);

  gab_vmpush(vm, *argv);
}

void gab_lib_put(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                 size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 3:
    // A put
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_put");
      return;
    }

    list_put(gab, gc, vm, argv[0], gab_valton(argv[1]), argv[2]);
    break;

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  gab_vmpush(vm, *argv);
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

void gab_lib_slice(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {
  v_gab_value *data = gab_boxdata(argv[0]);

  uint64_t len = data->len;
  uint64_t start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    uint64_t a = gab_valton(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER ||
        gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    uint64_t a = gab_valton(argv[1]);
    uint64_t b = gab_valton(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  uint64_t result_len = end - start;

  gab_value result = gab_tuple(gab, result_len, data->data + start);

  gab_vmpush(vm, result);

  gab_gcdref(gab, gc, vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  const char *names[] = {
      "list", "new", "len", "slice", "push!", "pop!", "put!", "at",
  };

  gab_value type = gab_string(gab, "List");

  gab_value receivers[] = {
      gab_nil, type, type, type, type, type, type, type,
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "list", gab_lib_new),
      gab_sbuiltin(gab, "new", gab_lib_new),
      gab_sbuiltin(gab, "len", gab_lib_len),
      gab_sbuiltin(gab, "slice", gab_lib_slice),
      gab_sbuiltin(gab, "push", gab_lib_push),
      gab_sbuiltin(gab, "pop", gab_lib_pop),
      gab_sbuiltin(gab, "put", gab_lib_put),
      gab_sbuiltin(gab, "at", gab_lib_at),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, gc, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = receivers[i],
                 .specialization = specs[i],
             });
  }

  return NULL;
}
