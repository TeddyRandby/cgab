#include "include/gab.h"
#include <assert.h>

gab_value gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_obj_list *list = gab_obj_list_create_empty(gab, 8);

    gab_value result = GAB_VAL_OBJ(list);

    gab_dref(gab, vm, result);

    return result;
  }
  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1]))
      return gab_panic(gab, vm, "Invalid call to gab_lib_new");

    u64 len = GAB_VAL_TO_NUMBER(argv[1]);

    gab_obj_list *list = gab_obj_list_create_empty(gab, len);

    while (len--)
      v_gab_value_push(&list->data, GAB_VAL_NIL());

    gab_value result = GAB_VAL_OBJ(list);

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

  gab_obj_list *obj = GAB_VAL_TO_LIST(argv[0]);

  return GAB_VAL_NUMBER(obj->data.len);
}

gab_value gab_lib_pop(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  if (argc != 1)
    return gab_panic(gab, vm, "Invalid call to gab_lib_len");

  gab_obj_list *obj = GAB_VAL_TO_LIST(argv[0]);

  gab_value result = v_gab_value_pop(&obj->data);

  gab_dref(gab, vm, result);

  return result;
}

gab_value gab_lib_push(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc < 2)
    return gab_panic(gab, vm, "Invalid call to gab_lib_len");

  gab_obj_list *obj = GAB_VAL_TO_LIST(argv[0]);

  for (u8 i = 1; i < argc; i++)
    v_gab_value_push(&obj->data, argv[i]);

  gab_iref_many(gab, vm, argc - 1, argv + 1);

  return argv[0];
}

gab_value gab_lib_at(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
  gab_obj_list *list = GAB_VAL_TO_LIST(argv[0]);

  if (argc != 2 || !GAB_VAL_IS_NUMBER(argv[1]))
    return gab_panic(gab, vm, "Invalid call to gab_lib_put");

  u64 offset = GAB_VAL_TO_NUMBER(argv[1]);

  return gab_obj_list_at(list, offset);
}

gab_value gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {
  gab_obj_list *list = GAB_VAL_TO_LIST(argv[0]);

  switch (argc) {
  case 3:
    // A put
    if (!GAB_VAL_IS_NUMBER(argv[1]))
      return gab_panic(gab, vm, "Invalid call to gab_lib_put");

    gab_obj_list_put(list, GAB_VAL_TO_NUMBER(argv[1]), argv[2]);

    break;

  default:
    return gab_panic(gab, vm, "Invalid call to gab_lib_put");
  }

  gab_iref_many(gab, vm, argc - 1, argv + 1);

  return argv[0];
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

gab_value gab_lib_slice(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  gab_obj_list *list = GAB_VAL_TO_LIST(argv[0]);

  u64 len = list->data.len;
  u64 start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      return GAB_VAL_NIL();
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3:
    if (!GAB_VAL_IS_NUMBER(argv[1]) || !GAB_VAL_TO_NUMBER(argv[2])) {
      return GAB_VAL_NIL();
    }
    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    u64 b = GAB_VAL_TO_NUMBER(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  default:
    return gab_panic(gab, vm, "Invalid call to gab_lib_slice");
  }

  u64 result_len = end - start;

  gab_obj_shape *shape = gab_obj_shape_create_tuple(gab, vm, result_len);

  gab_obj_record *rec =
      gab_obj_record_create(gab, shape, 1, list->data.data + start);

  gab_value result = GAB_VAL_OBJ(rec);

  gab_dref(gab, vm, result);

  return result;
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  s_i8 names[] = {
      s_i8_cstr("new"), s_i8_cstr("len"), s_i8_cstr("slice"), s_i8_cstr("push"),
      s_i8_cstr("pop"), s_i8_cstr("put"), s_i8_cstr("at"),
  };

  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_LIST), gab_type(gab, GAB_KIND_LIST),
      gab_type(gab, GAB_KIND_LIST), gab_type(gab, GAB_KIND_LIST),
      gab_type(gab, GAB_KIND_LIST), gab_type(gab, GAB_KIND_LIST),
      gab_type(gab, GAB_KIND_LIST),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new), GAB_BUILTIN(len), GAB_BUILTIN(slice), GAB_BUILTIN(push),
      GAB_BUILTIN(pop), GAB_BUILTIN(put), GAB_BUILTIN(at),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, names[i], receivers[i], specs[i]);
    gab_dref(gab, vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
