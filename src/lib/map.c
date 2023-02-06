#include "include/gab.h"
#include "include/object.h"
#include <assert.h>
#include <stdio.h>

gab_value gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_obj_map *map = gab_obj_map_create(0, 0, NULL, NULL);

    gab_value result = GAB_VAL_OBJ(map);

    gab_dref(gab, vm, result);

    return result;
  }

  case 2: {
    if (!GAB_VAL_IS_RECORD(argv[1]))
      return gab_panic(gab, vm, "Invalid call to gab_lib_len");

    gab_obj_record* rec = GAB_VAL_TO_RECORD(argv[1]);
    gab_obj_shape* shp = rec->shape;

    gab_iref_many(gab, vm, rec->len, rec->data);

    gab_iref_many(gab, vm, shp->len, shp->data);

    gab_obj_map* map = gab_obj_map_create(rec->len, 1, shp->data, rec->data);

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

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  s_i8 names[] = {
      s_i8_cstr("new"),
      s_i8_cstr("len"),
      s_i8_cstr("put"),
      s_i8_cstr("at"),
  };

  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_MAP),
      gab_type(gab, GAB_KIND_MAP),
      gab_type(gab, GAB_KIND_MAP),
      gab_type(gab, GAB_KIND_MAP),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new),
      GAB_BUILTIN(len),
      GAB_BUILTIN(put),
      GAB_BUILTIN(at),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, names[i], receivers[i], specs[i]);
    gab_dref(gab, vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
