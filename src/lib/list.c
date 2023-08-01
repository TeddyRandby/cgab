#include "list.h"
#include <assert.h>

void gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    gab_obj_container *list = list_create_empty(gab, vm, 8);

    gab_value result = GAB_VAL_OBJ(list);

    gab_push(vm, result);

    gab_val_dref(vm, result);

    return;
  }
  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    u64 len = GAB_VAL_TO_NUMBER(argv[1]);

    gab_obj_container *list = list_create_empty(gab, vm, len);

    while (len--)
      v_gab_value_push(list->data, GAB_VAL_NIL());

    gab_value result = GAB_VAL_OBJ(list);
    gab_push(vm, result);

    gab_val_dref(vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
}

void gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_obj_container *obj = GAB_VAL_TO_CONTAINER(argv[0]);

  gab_value result = GAB_VAL_NUMBER(((v_gab_value *)obj->data)->len);

  gab_push(vm, result);

  return;
}

void gab_lib_pop(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_obj_container *obj = GAB_VAL_TO_CONTAINER(argv[0]);

  gab_value result = v_gab_value_pop(obj->data);

  gab_push(vm, result);

  gab_val_dref(vm, result);

  return;
}

void gab_lib_push(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }

  gab_obj_container *obj = GAB_VAL_TO_CONTAINER(argv[0]);

  for (u8 i = 1; i < argc; i++)
    v_gab_value_push(obj->data, argv[i]);

  gab_val_viref(vm, argc - 1, argv + 1);

  gab_push(vm, *argv);
}

void gab_lib_at(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *list = GAB_VAL_TO_CONTAINER(argv[0]);

  if (argc != 2 || !GAB_VAL_IS_NUMBER(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  u64 offset = GAB_VAL_TO_NUMBER(argv[1]);

  gab_value res = list_at(list, offset);

  gab_push(vm, res);
}

void gab_lib_del(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *list = GAB_VAL_TO_CONTAINER(argv[0]);

  if (argc != 2 || !GAB_VAL_IS_NUMBER(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_del");
    return;
  }

  u64 index = GAB_VAL_TO_NUMBER(argv[1]);

  gab_val_dref(vm, v_gab_value_val_at(list->data, index));

  v_gab_value_del(list->data, index);

  gab_push(vm, *argv);
}

void gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *list = GAB_VAL_TO_CONTAINER(argv[0]);

  switch (argc) {
  case 3:
    // A put
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_put");
      return;
    }

    list_put(gab, vm, list, GAB_VAL_TO_NUMBER(argv[1]), argv[2]);

    break;

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  gab_push(vm, *argv);
}

// Boy do NOT put side effects in here
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))

void gab_lib_slice(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  gab_obj_container *list = GAB_VAL_TO_CONTAINER(argv[0]);

  v_gab_value *data = list->data;

  u64 len = data->len;
  u64 start = 0, end = len;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  case 3: {
    if (!GAB_VAL_IS_NUMBER(argv[1]) || !GAB_VAL_TO_NUMBER(argv[2])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    u64 b = GAB_VAL_TO_NUMBER(argv[2]);
    start = CLAMP(a, len);
    end = CLAMP(b, len);
    break;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  u64 result_len = end - start;

  gab_obj_shape *shape = gab_obj_shape_create_tuple(gab, vm, result_len);

  gab_obj_record *rec =
      gab_obj_record_create(gab, vm, shape, 1, data->data + start);

  gab_value result = GAB_VAL_OBJ(rec);

  gab_push(vm, result);

  gab_val_dref(vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("list"),  GAB_STRING("len"),  GAB_STRING("slice"),
      GAB_STRING("push!"), GAB_STRING("pop!"), GAB_STRING("put!"),
      GAB_STRING("at"),
  };

  gab_value type = GAB_STRING("List");

  gab_value receivers[] = {
      GAB_VAL_NIL(), type, type, type, type, type, type,
  };

  gab_value specs[] = {
      GAB_BUILTIN(new), GAB_BUILTIN(len), GAB_BUILTIN(slice), GAB_BUILTIN(push),
      GAB_BUILTIN(pop), GAB_BUILTIN(put), GAB_BUILTIN(at),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
