#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include "list.h"
#include "map.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void gab_lib_send(gab_engine *gab, gab_vm *vm, u8 argc,
                  gab_value argv[static argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");

    return;
  }

  a_gab_value *result = gab_send(gab, vm, argv[1], argv[0], argc - 2, argv + 2);

  if (!result) {
    gab_panic(gab, vm, "Invalid send");
    return;
  }

  gab_vpush(vm, result->len, result->data);

  a_gab_value_destroy(result);
}

void gab_lib_splat(gab_engine *gab, gab_vm *vm, u8 argc,
                   gab_value argv[static argc]) {
  if (!GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_splat");
    return;
  }

  gab_obj_record *rec = GAB_VAL_TO_RECORD(argv[0]);

  gab_vpush(vm, rec->len, rec->data);
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
    start = MAX(a, 0);

    u64 b = GAB_VAL_TO_NUMBER(argv[2]);
    end = MIN(b, len);
    break;
  }

  case 2:
    if (GAB_VAL_IS_NUMBER(argv[1])) {
      u64 a = GAB_VAL_TO_NUMBER(argv[1]);
      end = MIN(a, len);
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

  gab_push(vm, result);

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

  gab_push(vm, result);
}

void gab_lib_put(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 3 || !GAB_VAL_IS_RECORD(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  if (!gab_obj_record_put(vm, GAB_VAL_TO_RECORD(argv[0]), argv[1], argv[2])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_put");
    return;
  }

  gab_push(vm, argv[1]);
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
  gab_push(vm, res);
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

    gab_push(vm, result);

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

      gab_push(vm, result);

      return;
    }

    if (GAB_VAL_IS_SHAPE(argv[0])) {
      gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[0]);

      gab_value result = GAB_VAL_NUMBER(shape->len);

      gab_push(vm, result);

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

      gab_obj_container *list = list_create(gab, vm, rec->len, rec->data);

      gab_value result = GAB_VAL_OBJ(list);

      gab_push(vm, result);

      gab_val_dref(vm, result);

      return;
    }

    if (GAB_VAL_IS_SHAPE(argv[0])) {
      gab_obj_shape *shape = GAB_VAL_TO_SHAPE(argv[0]);

      gab_obj_container *list = list_create(gab, vm, shape->len, shape->data);

      gab_value result = GAB_VAL_OBJ(list);

      gab_push(vm, result);

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

void gab_lib_implements(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (!GAB_VAL_IS_MESSAGE(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_implements");
      return;
    }

    gab_value type = argv[0];

    gab_obj_message *msg = GAB_VAL_TO_MESSAGE(argv[1]);

    boolean implements = gab_obj_message_find(msg, type) != UINT64_MAX;

    gab_value result = GAB_VAL_BOOLEAN(implements);

    gab_push(vm, result);

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_implements");
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
    gab_obj_container *map =
        map_create(gab, vm, rec->len, 1, rec->shape->data, rec->data);

    gab_value result = GAB_VAL_OBJ(map);

    gab_push(vm, result);

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
      GAB_STRING("record"), GAB_STRING("len"),         GAB_STRING("to_l"),
      GAB_STRING("to_m"),   GAB_STRING("send"),        GAB_STRING("put"),
      GAB_STRING("at"),     GAB_STRING("next"),        GAB_STRING("slice"),
      GAB_STRING("splat"),  GAB_STRING("implements?"),
  };

  gab_value receivers[] = {
      GAB_VAL_NIL(),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_UNDEFINED),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_RECORD),
      gab_type(gab, kGAB_UNDEFINED),
  };

  gab_value specs[] = {
      GAB_BUILTIN(new),   GAB_BUILTIN(len),        GAB_BUILTIN(to_l),
      GAB_BUILTIN(to_m),  GAB_BUILTIN(send),       GAB_BUILTIN(put),
      GAB_BUILTIN(at),    GAB_BUILTIN(next),       GAB_BUILTIN(slice),
      GAB_BUILTIN(splat), GAB_BUILTIN(implements),

  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], specs[i]);
    gab_val_dref(vm, specs[i]);
  }

  return GAB_VAL_NIL();
}
