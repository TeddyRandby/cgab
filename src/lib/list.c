#include "list.h"
#include "include/gab.h"
#include <assert.h>

void gab_lib_new(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list = list_create_empty(gab, vm, 8);

    gab_vmpush(vm, list);

    gab_gcdref(gab_vmgc(vm), vm, list);

    return;
  }
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    u64 len = gab_valton(argv[1]);

    gab_value list = list_create_empty(gab, vm, len * 2);

    while (len--)
      v_gab_value_push(gab_boxdata(list), gab_nil);

    gab_vmpush(vm, list);

    gab_gcdref(gab_vmgc(vm), vm, list);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
}

void gab_lib_len(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_value result = gab_number(((v_gab_value *)gab_boxdata(argv[0]))->len);

  gab_vmpush(vm, result);

  return;
}

void gab_lib_pop(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_value result = v_gab_value_pop(gab_boxdata(argv[0]));

  gab_vmpush(vm, result);

  gab_gcdref(gab_vmgc(vm), vm, result);

  return;
}

void gab_lib_push(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  for (u8 i = 1; i < argc; i++)
    v_gab_value_push(gab_boxdata(argv[0]), argv[i]);

  gab_ngciref(gab_vmgc(vm), vm, argc - 1, argv + 1);

  gab_vmpush(vm, *argv);
}

void gab_lib_at(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  u64 offset = gab_valton(argv[1]);

  gab_value res = list_at(argv[0], offset);

  gab_vmpush(vm, res);
}

void gab_lib_del(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_del");
    return;
  }

  u64 index = gab_valton(argv[1]);

  gab_gcdref(gab_vmgc(vm), vm, v_gab_value_val_at(gab_boxdata(argv[0]), index));

  v_gab_value_del(gab_boxdata(argv[0]), index);

  gab_vmpush(vm, *argv);
}

void gab_lib_put(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 3:
    // A put
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_put");
      return;
    }

    list_put(gab, vm, argv[0], gab_valton(argv[1]), argv[2]);

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

void gab_lib_slice(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  v_gab_value *data = gab_boxdata(argv[0]);

  u64 len = data->len;
  u64 start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = gab_valton(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER ||
        gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = gab_valton(argv[1]);
    u64 b = gab_valton(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  u64 result_len = end - start;

  gab_value result = gab_tuple(gab, vm, result_len, data->data + start);

  gab_vmpush(vm, result);

  gab_gcdref(gab_vmgc(vm), vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "list", "len", "slice", "push!", "pop!", "put!", "at",
  };

  gab_value type = gab_string(gab, "List");

  gab_value receivers[] = {
      gab_nil, type, type, type, type, type, type,
  };

  gab_value specs[] = {
      gab_builtin(gab, "new", gab_lib_new),
      gab_builtin(gab, "len", gab_lib_len),
      gab_builtin(gab, "slice", gab_lib_slice),
      gab_builtin(gab, "push", gab_lib_push),
      gab_builtin(gab, "pop", gab_lib_pop),
      gab_builtin(gab, "put", gab_lib_put),
      gab_builtin(gab, "at", gab_lib_at),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .specialization = specs[i],
                 .receiver = receivers[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(names), specs);

  return NULL;
}
