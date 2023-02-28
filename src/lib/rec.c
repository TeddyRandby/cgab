#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include <assert.h>

gab_value gab_lib_new(gab_engine *gab, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (!GAB_VAL_IS_SHAPE(argv[1]))
      return gab_panic(gab, "Invalid call to gab_lib_new");

    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[1]);

    gab_obj_record *rec = gab_obj_record_create_empty(gab, shape);

    gab_value result = GAB_VAL_OBJ(rec);

    gab_dref(gab, result);

    return result;
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_new");
  }
};

gab_value gab_lib_len(gab_engine *gab, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    if (GAB_VAL_IS_RECORD(argv[0])) {
      gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

      gab_value result = GAB_VAL_NUMBER(rec->len);

      return result;
    }

    if (GAB_VAL_IS_SHAPE(argv[0])) {
      gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[0]);

      gab_value result = GAB_VAL_NUMBER(shape->len);

      return result;
    }

    return gab_panic(gab, "Invalid call to gab_lib_len");
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_len");
  }
}

gab_value gab_lib_to_l(gab_engine *gab, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    if (GAB_VAL_IS_RECORD(argv[0])) {
      gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

      gab_obj_list *list = gab_obj_list_create(gab, rec->len, 1, rec->data);

      gab_value result = GAB_VAL_OBJ(list);

      gab_dref(gab, result);

      return result;
    }

    if (GAB_VAL_IS_SHAPE(argv[0])) {
      gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[0]);

      gab_obj_list *list = gab_obj_list_create(gab, shape->len, 1, shape->data);

      gab_value result = GAB_VAL_OBJ(list);

      gab_dref(gab, result);

      return result;
    }

    return gab_panic(gab, "Invalid call to gab_lib_to_l");
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_l");
  }
}

gab_value gab_lib_to_m(gab_engine *gab, u8 argc, gab_value argv[argc]) {
  if (!GAB_VAL_IS_RECORD(argv[0]))
    return gab_panic(gab, "Invalid call to gab_lib_to_l");

  gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

  switch (argc) {
  case 1: {
    gab_obj_map *map =
        gab_obj_map_create(gab, rec->len, 1, rec->shape->data, rec->data);

    gab_value result = GAB_VAL_OBJ(map);

    gab_dref(gab, result);

    return result;
  }

  default:
    return gab_panic(gab, "Invalid call to gab_lib_to_m");
  }
}
gab_value gab_mod(gab_engine *gab) {
  gab_value names[] = {
      GAB_STRING("new"),
      GAB_STRING("len"),
      GAB_STRING("to_l"),
      GAB_STRING("to_m"),
  };

  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_RECORD),
      gab_type(gab, GAB_KIND_UNDEFINED),
      gab_type(gab, GAB_KIND_UNDEFINED),
      gab_type(gab, GAB_KIND_UNDEFINED),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new),
      GAB_BUILTIN(len),
      GAB_BUILTIN(to_l),
      GAB_BUILTIN(to_m),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, names[i], receivers[i], specs[i]);
    gab_dref(gab, specs[i]);
  }

  return GAB_VAL_NIL();
}
