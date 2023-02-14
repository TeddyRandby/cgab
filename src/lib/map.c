#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include <assert.h>
#include <stdio.h>

gab_value gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_obj_map *map = gab_obj_map_create(gab, 0, 0, NULL, NULL);

    gab_value result = GAB_VAL_OBJ(map);

    gab_dref(gab, vm, result);

    return result;
  }

  case 2: {
    if (!GAB_VAL_IS_RECORD(argv[1]))
      return gab_panic(gab, vm, "Invalid call to gab_lib_len");

    gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[1]);
    gab_obj_shape *shp = rec->shape;

    gab_iref_many(gab, vm, rec->len, rec->data);

    gab_iref_many(gab, vm, shp->len, shp->data);

    gab_obj_map *map =
        gab_obj_map_create(gab, rec->len, 1, shp->data, rec->data);

    gab_value result = GAB_VAL_OBJ(map);

    gab_dref(gab, vm, result);

    return result;
  }
  default:
    return gab_panic(gab, vm, "Invalid call to gab_lib_new");
  }
}

gab_value gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 1)
    return gab_panic(gab, vm, "Invalid call to gab_lib_len");

  gab_obj_map *obj = GAB_VAL_TO_MAP(argv[0]);

  return GAB_VAL_NUMBER(obj->data.len);
}

gab_value gab_lib_at(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
  if (argc != 2)
    return gab_panic(gab, vm, "Invalid call to gab_lib_at");

  gab_obj_map *map = GAB_VAL_TO_MAP(argv[0]);

  return gab_obj_map_at(map, argv[1]);
}

gab_value gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 3)
    return gab_panic(gab, vm, "Invalid call to gab_lib_put");

  gab_obj_map *map = GAB_VAL_TO_MAP(argv[0]);

  gab_obj_map_put(map, argv[1], argv[2]);

  gab_iref_many(gab, vm, 2, argv + 1); // Increment the key and value

  return GAB_VAL_NIL();
}

gab_value gab_lib_next(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  gab_obj_map *map = GAB_VAL_TO_MAP(argv[0]);

  switch (argc) {
  case 1: {
    u64 next_index = d_gab_value_inext(&map->data, 0);

    if (next_index == -1)
      return GAB_VAL_NIL();

    return d_gab_value_ikey(&map->data, next_index);
  }
  case 2: {

    gab_value key = argv[1];

    u64 index = d_gab_value_index_of(&map->data, key);

    u64 next_index = d_gab_value_inext(&map->data, index + 1);

    if (next_index == -1)
      return GAB_VAL_NIL();

    return d_gab_value_ikey(&map->data, next_index);
  }
  default:
    return gab_panic(gab, vm, "Invalid call to gab_lib_next");
  }
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("new"), GAB_STRING("len"),  GAB_STRING("put"),
      GAB_STRING("at"),  GAB_STRING("next"),
  };

  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_MAP), gab_type(gab, GAB_KIND_MAP),
      gab_type(gab, GAB_KIND_MAP), gab_type(gab, GAB_KIND_MAP),
      gab_type(gab, GAB_KIND_MAP),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new), GAB_BUILTIN(len),  GAB_BUILTIN(put),
      GAB_BUILTIN(at),  GAB_BUILTIN(next),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, names[i], receivers[i], specs[i]);
    gab_dref(gab, vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
