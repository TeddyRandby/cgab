#include "list.h"
#include "include/gab.h"
#include <assert.h>

void gab_lib_new(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value list = list_create_empty(gab, 8);

    gab_vmpush(gab.vm, list);

    return;
  }
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, "Invalid call to gab_lib_new");
      return;
    }

    uint64_t len = gab_valton(argv[1]);

    gab_value list = list_create_empty(gab, len * 2);

    while (len--)
      v_gab_value_push(gab_boxdata(list), gab_nil);

    gab_vmpush(gab.vm, list);

    return;
  }
  default:
    gab_panic(gab, "Invalid call to gab_lib_new");
    return;
  }
}

void gab_lib_len(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, "Invalid call to gab_lib_len");
    return;
  }

  gab_value result = gab_number(((v_gab_value *)gab_boxdata(argv[0]))->len);

  gab_vmpush(gab.vm, result);

  return;
}

void gab_lib_pop(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, "Invalid call to gab_lib_len");
    return;
  }

  gab_value result = v_gab_value_pop(gab_boxdata(argv[0]));

  gab_vmpush(gab.vm, result);

  return;
}

void gab_lib_push(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc < 2) {
    gab_panic(gab, "Invalid call to gab_lib_len");
    return;
  }

  for (uint8_t i = 1; i < argc; i++)
    v_gab_value_push(gab_boxdata(argv[0]), argv[i]);

  gab_vmpush(gab.vm, *argv);
}

void gab_lib_at(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, "Invalid call to gab_lib_put");
    return;
  }

  uint64_t offset = gab_valton(argv[1]);

  gab_value res = list_at(argv[0], offset);

  gab_vmpush(gab.vm, res);
}

void gab_lib_del(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, "Invalid call to gab_lib_del");
    return;
  }

  uint64_t index = gab_valton(argv[1]);

  v_gab_value_del(gab_boxdata(argv[0]), index);

  gab_vmpush(gab.vm, *argv);
}

void gab_lib_put(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 3:
    // A put
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, "Invalid call to gab_lib_put");
      return;
    }

    list_put(gab, argv[0], gab_valton(argv[1]), argv[2]);
    break;

  default:
    gab_panic(gab, "Invalid call to gab_lib_put");
    return;
  }

  gab_vmpush(gab.vm, *argv);
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

void gab_lib_slice(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  v_gab_value *data = gab_boxdata(argv[0]);

  uint64_t len = data->len;
  uint64_t start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, "Invalid call to gab_lib_slice");
      return;
    }

    uint64_t a = gab_valton(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER ||
        gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, "Invalid call to gab_lib_slice");
      return;
    }

    uint64_t a = gab_valton(argv[1]);
    uint64_t b = gab_valton(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  }
  default:
    gab_panic(gab, "Invalid call to gab_lib_slice");
    return;
  }

  uint64_t result_len = end - start;

  gab_value result = gab_tuple(gab.eg, result_len, data->data + start);

  gab_vmpush(gab.vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value type = gab_string(gab.eg, "List");
  const char *names[] = {};

  struct gab_spec_argt specs[] = {
      {
          "list",
          gab_nil,
          gab_sbuiltin(gab.eg, "list", gab_lib_new),
      },
      {
          "new",
          type,
          gab_sbuiltin(gab.eg, "new", gab_lib_new),
      },
      {
          "len",
          type,
          gab_sbuiltin(gab.eg, "len", gab_lib_len),
      },
      {
          "slice",
          type,
          gab_sbuiltin(gab.eg, "slice", gab_lib_slice),
      },
      {
          "push!",
          type,
          gab_sbuiltin(gab.eg, "push", gab_lib_push),
      },
      {
          "pop!",
          type,
          gab_sbuiltin(gab.eg, "pop", gab_lib_pop),
      },
      {
          "put!",
          type,
          gab_sbuiltin(gab.eg, "put", gab_lib_put),
      },
      {
          "at",
          type,
          gab_sbuiltin(gab.eg, "at", gab_lib_at),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
