
#include "include/gab.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

typedef struct {
  u32 state[16];
  u32 index;
  boolean seeded;
} Well512;

static void random_seed(Well512 *well) {
  srand((uint32_t)time(NULL));
  for (int i = 0; i < 16; i++) {
    well->state[i] = rand();
  }
}

// Code from: http://www.lomont.org/Math/Papers/2008/Lomont_PRNG_2008.pdf
static uint32_t advanceState(Well512 *well) {
  uint32_t a, b, c, d;
  a = well->state[well->index];
  c = well->state[(well->index + 13) & 15];
  b = a ^ c ^ (a << 16) ^ (c << 15);
  c = well->state[(well->index + 9) & 15];
  c ^= (c >> 11);
  a = well->state[well->index] = b ^ c;
  d = a ^ ((a << 5) & 0xda442d24U);

  well->index = (well->index + 15) & 15;
  a = well->state[well->index];
  well->state[well->index] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);
  return well->state[well->index];
}

static f64 random_float() {
  static Well512 well;

  if (!well.seeded) {
    well.seeded = true;
    random_seed(&well);
  }

  // A double has 53 bits of precision in its mantissa, and we'd like to take
  // full advantage of that, so we need 53 bits of random source data.

  // First, start with 32 random bits, shifted to the left 21 bits.
  f64 result = (f64)advanceState(&well) * (1 << 21);

  // Then add another 21 random bits.
  result += (f64)(advanceState(&well) & ((1 << 21) - 1));

  // Now we have a number from 0 - (2^53). Divide be the range to get a double
  // from 0 to 1.0 (half-inclusive).
  result /= 9007199254740992.0;

  return result;
}

void gab_lib_between(gab_engine *gab, gab_vm *vm, u8 argc,
                          gab_value argv[argc]) {

  f64 min = 0, max = 1;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_random");

      return;
    }

    max = GAB_VAL_TO_NUMBER(argv[1]);

    break;
  }

  case 3: {
    if (!GAB_VAL_IS_NUMBER(argv[1]) || !GAB_VAL_IS_NUMBER(argv[2])) {
      gab_panic(gab, vm, "Invalid call to gab_lib_random");

      return;
    }

    min = GAB_VAL_TO_NUMBER(argv[1]);
    max = GAB_VAL_TO_NUMBER(argv[2]);
    break;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_random");
  }

  f64 num = min + (random_float() * max);

  gab_value res = GAB_VAL_NUMBER(num);

  gab_push(vm, 1, &res);
}

void gab_lib_floor(gab_engine *gab, gab_vm *vm, u8 argc,
                        gab_value argv[argc]) {

  if (argc != 1 || !GAB_VAL_IS_NUMBER(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_floor");

    return;
  }

  f64 float_num = GAB_VAL_TO_NUMBER(argv[0]);
  i64 int_num = GAB_VAL_TO_NUMBER(argv[0]);

  gab_value res = GAB_VAL_NUMBER(int_num + (float_num < 0));

  gab_push(vm, 1, &res);
}

void gab_lib_to_n(gab_engine *gab, gab_vm *vm, u8 argc,
                       gab_value argv[argc]) {
  if (argc != 1 || !GAB_VAL_IS_STRING(argv[0])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_from");

    return;
  }

  gab_obj_string *str = GAB_VAL_TO_STRING(argv[0]);

  char cstr[str->len + 1];

  memcpy(cstr, str->data, str->len);

  cstr[str->len] = '\0';

  gab_value res = GAB_VAL_NUMBER(strtod(cstr, NULL));

  gab_push(vm, 1, &res);
};

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("between"),
      GAB_STRING("floor"),
      GAB_STRING("to_n"),
  };

  gab_value receivers[] = {
      gab_type(gab, GAB_KIND_NUMBER),
      gab_type(gab, GAB_KIND_NUMBER),
      gab_type(gab, GAB_KIND_STRING),
  };

  gab_value values[] = {
      GAB_BUILTIN(between),
      GAB_BUILTIN(floor),
      GAB_BUILTIN(to_n),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(values));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));

  for (int i = 0; i < LEN_CARRAY(names); i++) {
    gab_specialize(gab, vm, names[i], receivers[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  return GAB_VAL_NIL();
}
