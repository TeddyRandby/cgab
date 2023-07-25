#include "map.h"
#include <assert.h>
#include <stdio.h>

void gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_obj_container *map = map_create(gab, vm, 0, 0, NULL, NULL);

    gab_value result = GAB_VAL_OBJ(map);

    gab_push(vm, 1, &result);

    gab_val_dref(vm, result);

    return;
  }

  case 2: {
    if (!GAB_VAL_IS_RECORD(argv[1])) {
      gab_panic(gab, vm, "&:new expects a record as second argument");
      return;
    }

    gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[1]);
    gab_obj_shape *shp = rec->shape;

    gab_obj_container *map =
        map_create(gab, vm, rec->len, 1, shp->data, rec->data);

    gab_value result = GAB_VAL_OBJ(map);

    gab_push(vm, 1, &result);

    gab_val_dref(vm, result);

    return;
  }

  default:
    gab_panic(gab, vm, "&:new expects 1 or 2 arguments");
    return;
  }
}

void gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "&:len expects 1 argument");
    return;
  }

  gab_obj_container *obj = GAB_VAL_TO_CONTAINER(argv[0]);

  d_gab_value *data = obj->data;

  gab_value res = GAB_VAL_NUMBER(data->len);

  gab_push(vm, 1, &res);

  return;
}

void gab_lib_at(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2) {
    gab_panic(gab, vm, "&:at expects 2 arguments");
    return;
  }

  gab_obj_container *map = GAB_VAL_TO_CONTAINER(argv[0]);

  gab_value res = map_at(map, argv[1]);

  if (GAB_VAL_IS_UNDEFINED(res)) {
    gab_value tag = GAB_STRING("none");
    gab_push(vm, 1, &tag);
    return;
  }

  gab_value tag = GAB_STRING("some");
  gab_push(vm, 1, &tag);
  gab_push(vm, 1, &res);
  return;
}

void gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 3) {
    gab_panic(gab, vm, "&:put! expects 3 arguments");
    return;
  }

  gab_obj_container *map = GAB_VAL_TO_CONTAINER(argv[0]);

  gab_value key = argv[1];

  map_put(gab, vm, map, key, argv[2]);

  gab_push(vm, 1, argv);

  return;
}

void gab_lib_next(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *map = GAB_VAL_TO_CONTAINER(argv[0]);

  switch (argc) {

  case 1: {
    u64 next_index = d_gab_value_inext(map->data, 0);

    if (next_index == -1) {
      gab_value res = GAB_VAL_NIL();

      gab_push(vm, 1, &res);

      return;
    }

    gab_value res = d_gab_value_ikey(map->data, next_index);

    gab_push(vm, 1, &res);

    return;
  }

  case 2: {

    gab_value key = argv[1];

    u64 index = d_gab_value_index_of(map->data, key);

    u64 next_index = d_gab_value_inext(map->data, index + 1);

    if (next_index == -1) {
      gab_value res = GAB_VAL_NIL();

      gab_push(vm, 1, &res);

      return;
    }

    gab_value res = d_gab_value_ikey(map->data, next_index);

    gab_push(vm, 1, &res);

    return;
  }

  default:
    gab_panic(gab, vm, "&:next expects 1 or 2 arguments");
    return;
  }
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("map"), GAB_STRING("len"),  GAB_STRING("put!"),
      GAB_STRING("at"),  GAB_STRING("next"),
  };

  gab_value type = GAB_STRING("Map");

  gab_value receivers[] = {
      GAB_VAL_NIL(), type, type, type, type,
  };

  gab_value specs[] = {
      GAB_BUILTIN(new), GAB_BUILTIN(len),  GAB_BUILTIN(put),
      GAB_BUILTIN(at),  GAB_BUILTIN(next),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
