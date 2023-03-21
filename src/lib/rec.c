#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <assert.h>
#include <stdint.h>

void gab_lib_send(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");

    return;
  }

  gab_value result = gab_send(gab, vm, argv[1], argv[0], argc - 2, argv + 2);

  if (GAB_VAL_IS_UNDEFINED(result)) {
    gab_panic(gab, vm, "Invalid send");

    return;
  }

  gab_push(vm, 1, &result);

  gab_val_dref(vm, result);
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (a < 0 ? 0 : MIN(a, b))
void gab_lib_slice(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

  u64 len = rec->len;
  u64 start = 0, end = len;

  switch (argc) {
  case 1:
    break;
  case 3: {
    if (!GAB_VAL_IS_NUMBER(argv[1]) || !GAB_VAL_IS_NUMBER(argv[2])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_slice");
      return;
    }

    u64 a = GAB_VAL_TO_NUMBER(argv[1]);
    start = CLAMP(a, len);
    break;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_slice");
    return;
  }

  u64 result_len = end - start;

  gab_obj_shape *shape = gab_obj_shape_create_tuple(gab, vm, result_len);

  gab_obj_record *val =
      gab_obj_record_create(gab, vm, shape, 1, rec->data + start);

  gab_value result = GAB_VAL_OBJ(val);

  gab_push(vm, 1, &result);

  gab_val_dref(vm, result);
}
#undef MIN
#undef MAX
#undef CLAMP

void gab_lib_at(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to  gab_lib_at");
    return;
  }

  gab_value result = gab_obj_record_at(GAB_VAL_TO_RECORD(argv[0]), argv[1]);

  gab_push(vm, 1, &result);
}

void gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 3 || !GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  if (!gab_obj_record_put(gab, vm, GAB_VAL_TO_RECORD(argv[0]), argv[1],
                          argv[2])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  gab_push(vm, 1, argv + 1);
}

void gab_lib_next(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_next");
    return;
  }

  gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

  gab_value res = GAB_VAL_NIL();

  if (rec->len < 1)
    goto fin;

  switch (argc) {
  case 1:
    res = rec->shape->data[0];
    goto fin;
  case 2: {
    u16 current = gab_obj_shape_find(rec->shape, argv[1]);

    if (current == UINT16_MAX || current + 1 == rec->len)
      goto fin;

    res = rec->shape->data[current + 1];
    goto fin;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_next");
    return;
  }

fin:
  gab_push(vm, 1, &res);
}

void gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (!GAB_VAL_IS_SHAPE(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_new");
      return;
    }

    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[1]);

    gab_obj_record *rec = gab_obj_record_create_empty(gab, shape);

    gab_value result = GAB_VAL_OBJ(rec);

    gab_push(vm, 1, &result);

    gab_val_dref(vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_new");
    return;
  }
};

void gab_lib_len(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    if (GAB_VAL_IS_RECORD(argv[0])) {
      gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

      gab_value result = GAB_VAL_NUMBER(rec->len);

      gab_push(vm, 1, &result);

      return;
    }

    if (GAB_VAL_IS_SHAPE(argv[0])) {
      gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[0]);

      gab_value result = GAB_VAL_NUMBER(shape->len);

      gab_push(vm, 1, &result);

      return;
    }

    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_len");
    return;
  }
}

void gab_lib_to_l(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  switch (argc) {
  case 1: {
    if (GAB_VAL_IS_RECORD(argv[0])) {
      gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

      gab_obj_list *list = gab_obj_list_create(gab, vm, rec->len, 1, rec->data);

      gab_value result = GAB_VAL_OBJ(list);

      gab_push(vm, 1, &result);

      gab_val_dref(vm, result);

      return;
    }

    if (GAB_VAL_IS_SHAPE(argv[0])) {
      gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[0]);

      gab_obj_list *list =
          gab_obj_list_create(gab, vm, shape->len, 1, shape->data);

      gab_value result = GAB_VAL_OBJ(list);

      gab_push(vm, 1, &result);

      gab_val_dref(vm, result);

      return;
    }

    gab_panic(gab, vm, "Invalid call to gab_lib_to_l");
    return;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_to_l");
    return;
  }
}

void gab_lib_to_m(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_to_l");
    return;
  }

  gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

  switch (argc) {
  case 1: {
    gab_obj_map *map =
        gab_obj_map_create(gab, vm, rec->len, 1, rec->shape->data, rec->data);

    gab_value result = GAB_VAL_OBJ(map);

    gab_push(vm, 1, &result);

    gab_val_dref(vm, result);

    return;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_to_m");
    return;
  }
}
gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("new"),  GAB_STRING("len"),  GAB_STRING("to_l"),
      GAB_STRING("to_m"), GAB_STRING("send"), GAB_STRING("put"),
      GAB_STRING("at"),   GAB_STRING("next"), GAB_STRING("slice"),
  };

  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_RECORD),    gab_type(gab, GAB_KIND_UNDEFINED),
      gab_type(gab, GAB_KIND_UNDEFINED), gab_type(gab, GAB_KIND_UNDEFINED),
      gab_type(gab, GAB_KIND_UNDEFINED), gab_type(gab, GAB_KIND_UNDEFINED),
      gab_type(gab, GAB_KIND_UNDEFINED), gab_type(gab, GAB_KIND_UNDEFINED),
      gab_type(gab, GAB_KIND_UNDEFINED),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new),  GAB_BUILTIN(len),  GAB_BUILTIN(to_l),
      GAB_BUILTIN(to_m), GAB_BUILTIN(send), GAB_BUILTIN(put),
      GAB_BUILTIN(at),   GAB_BUILTIN(next), GAB_BUILTIN(slice),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
