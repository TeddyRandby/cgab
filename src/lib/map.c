#include "map.h"
#include <assert.h>
#include <stdio.h>

void gab_lib_new(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_value map = map_create(gab, vm, 0, 0, NULL, NULL);

    gab_vmpush(vm, map);

    gab_gcdref(gab_vmgc(vm), vm, map);

    return;
  }

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_RECORD) {
      gab_panic(gab, vm, "&:new expects a record as second argument");
      return;
    }

    gab_value shp = gab_recshp(argv[1]);

    size_t len = gab_reclen(argv[1]);

    gab_value map =
        map_create(gab, vm, len, 1, gab_shpdata(shp), gab_recdata(argv[1]));

    gab_vmpush(vm, map);

    gab_gcdref(gab_vmgc(vm), vm, map);

    return;
  }

  default:
    gab_panic(gab, vm, "&:new expects 1 or 2 arguments");
    return;
  }
}

void gab_lib_len(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "&:len expects 1 argument");
    return;
  }

  d_gab_value *data = gab_boxdata(argv[0]);

  gab_value res = gab_number(data->len);

  gab_vmpush(vm, res);

  return;
}

void gab_lib_at(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "&:at expects 2 arguments");
    return;
  }

  gab_value res = map_at(argv[0], argv[1]);

  if (res == gab_undefined) {
    gab_vmpush(vm, gab_string(gab, "none"));
    return;
  }

  gab_vmpush(vm, gab_string(gab, "some"));
  gab_vmpush(vm, res);
  return;
}

void gab_lib_put(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 3) {
    gab_panic(gab, vm, "&:put! expects 3 arguments");
    return;
  }

  map_put(gab, vm, argv[0], argv[1], argv[2]);

  gab_vmpush(vm, *argv);

  return;
}

void gab_lib_next(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  d_gab_value *map = gab_boxdata(argv[0]);

  switch (argc) {

  case 1: {
    u64 next_index = d_gab_value_inext(map, 0);

    if (next_index == -1) {
      gab_value res = gab_nil;

      gab_vmpush(vm, res);

      return;
    }

    gab_value res = d_gab_value_ikey(map, next_index);

    gab_vmpush(vm, res);

    return;
  }

  case 2: {

    gab_value key = argv[1];

    u64 index = d_gab_value_index_of(map, key);

    u64 next_index = d_gab_value_inext(map, index + 1);

    if (next_index == -1) {
      gab_value res = gab_nil;

      gab_vmpush(vm, res);

      return;
    }

    gab_value res = d_gab_value_ikey(map, next_index);

    gab_vmpush(vm, res);

    return;
  }

  default:
    gab_panic(gab, vm, "&:next expects 1 or 2 arguments");
    return;
  }
}

a_gab_value* gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "map", "len", "put!", "at", "next",
  };

  gab_value type = gab_string(gab, "Map");

  gab_value receivers[] = {
      gab_nil, type, type, type, type,
  };

  gab_value specs[] = {
      gab_builtin(gab, "new", gab_lib_new),
      gab_builtin(gab, "len", gab_lib_len),
      gab_builtin(gab, "put", gab_lib_put),
      gab_builtin(gab, "at", gab_lib_at),
      gab_builtin(gab, "next", gab_lib_next),
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
