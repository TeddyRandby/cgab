#include "list.h"
#include "gab.h"

a_gab_value *gab_lib_new(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list = list_create_empty(gab, 8);

    gab_vmpush(gab.vm, list);
    return NULL;
  }
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to gab_lib_new");
    }

    uint64_t len = gab_valton(argv[1]);

    gab_value list = list_create_empty(gab, len * 2);

    while (len--)
      v_gab_value_push(gab_boxdata(list), gab_nil);

    gab_vmpush(gab.vm, list);
    return NULL;
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_new");
  }
}

a_gab_value *gab_lib_to_bytes(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "Invalid call to gab_lib_to_bytes");
  }

  v_gab_value *bytes = gab_boxdata(argv[0]);

  char buffer[bytes->len];

  for (uint64_t i = 0; i < bytes->len; i++) {
    if (gab_valkind(bytes->data[i]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to gab_lib_to_bytes");
    }

    double val = gab_valton(bytes->data[i]);

    if (val > 255 || val < 0) {
      return gab_panic(gab, "Invalid call to gab_lib_to_bytes");
    }

    buffer[i] = val;
  }

  gab_value result = gab_nstring(gab, bytes->len, buffer);
  gab_vmpush(gab.vm, result);
  return NULL;
}

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "Invalid call to gab_lib_len");
  }

  gab_value result = gab_number(((v_gab_value *)gab_boxdata(argv[0]))->len);

  gab_vmpush(gab.vm, result);
  return NULL;
}

a_gab_value *gab_lib_pop(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "Invalid call to gab_lib_len");
  }

  gab_value result = list_pop(gab, argv[0]);

  gab_vmpush(gab.vm, result);
  return NULL;
}

a_gab_value *gab_lib_push(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc < 2) {
    return gab_panic(gab, "Invalid call to gab_lib_push");
  }

  list_push(gab, argv[0], argc - 1, argv + 1);

  gab_vmpush(gab.vm, *argv);
  return NULL;
}

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 2 || gab_valkind(argv[1]) != kGAB_NUMBER) {
    return gab_panic(gab, "Invalid call to gab_lib_at");
  }

  uint64_t offset = gab_valton(argv[1]);

  gab_value res = list_at(argv[0], offset);

  gab_vmpush(gab.vm, res);
  return NULL;
}

a_gab_value *gab_lib_del(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 2 || gab_valkind(argv[1]) != kGAB_NUMBER) {
    return gab_panic(gab, "Invalid call to gab_lib_del");
  }

  uint64_t index = gab_valton(argv[1]);

  v_gab_value_del(gab_boxdata(argv[0]), index);

  gab_vmpush(gab.vm, *argv);
  return NULL;
}

a_gab_value *gab_lib_splat(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  v_gab_value *v = gab_boxdata(argv[0]);
  gab_nvmpush(gab.vm, v->len, v->data);
  return NULL;
}

a_gab_value *gab_lib_set(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  switch (argc) {
  case 2:
    if (gab_valkind(argv[1]) != kGAB_BOX) {
      return gab_panic(gab, "Invalid call to gab_lib_set");
    }

    if (gab_boxtype((argv[1])) != gab_string(gab, "gab.list")) {
      return gab_panic(gab, "Invalid call to gab_lib_set");
    }

    list_replace(gab, argv[0], argv[1]);
    break;

  default:
    return gab_panic(gab, "Invalid call to gab_lib_set");
  }

  gab_vmpush(gab.vm, *argv);
  return NULL;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  switch (argc) {
  case 3:
    // A put
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to gab_lib_put");
    }

    list_put(gab, argv[0], gab_valton(argv[1]), argv[2]);
    break;

  default:
    return gab_panic(gab, "Invalid call to gab_lib_put");
  }

  gab_vmpush(gab.vm, *argv);
  return NULL;
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

a_gab_value *gab_lib_slice(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  v_gab_value *data = gab_boxdata(argv[0]);

  uint64_t len = data->len;
  uint64_t start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to gab_lib_slice");
    }

    uint64_t a = gab_valton(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER ||
        gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_panic(gab, "Invalid call to gab_lib_slice");
    }

    uint64_t a = gab_valton(argv[1]);
    uint64_t b = gab_valton(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_slice");
  }

  uint64_t result_len = end - start;

  gab_value result = gab_tuple(gab, result_len, data->data + start);

  gab_vmpush(gab.vm, result);
  return NULL;
}
#undef MIN
#undef MAX
#undef CLAMP

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_strtosig(gab_string(gab, "gab.list"));

  struct gab_spec_argt specs[] = {
      {
          "list.new",
          gab_undefined,
          gab_snative(gab, "list.new", gab_lib_new),
      },
      {
          "new",
          type,
          gab_snative(gab, "list.new", gab_lib_new),
      },
      {
          "len",
          type,
          gab_snative(gab, "list.len", gab_lib_len),
      },
      {
          "slice",
          type,
          gab_snative(gab, "list.slice", gab_lib_slice),
      },
      {
          "push!",
          type,
          gab_snative(gab, "list.push", gab_lib_push),
      },
      {
          "pop!",
          type,
          gab_snative(gab, "list.pop", gab_lib_pop),
      },
      {
          "put!",
          type,
          gab_snative(gab, "list.put", gab_lib_put),
      },
      {
          "replace!",
          type,
          gab_snative(gab, "list.set", gab_lib_set),
      },
      {
          mGAB_SPLAT,
          type,
          gab_snative(gab, "list.splat", gab_lib_splat),
      },
      {
          "at",
          type,
          gab_snative(gab, "list.at", gab_lib_at),
      },
      {
          "to_bytes",
          type,
          gab_snative(gab, "list.to_bytes", gab_lib_to_bytes),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
