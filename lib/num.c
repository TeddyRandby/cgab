
#include "include/gab.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

typedef struct {
  uint32_t state[16];
  uint32_t index;
  bool seeded;
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

static double random_float() {
  static Well512 well;

  if (!well.seeded) {
    well.seeded = true;
    random_seed(&well);
  }

  // A double has 53 bits of precision in its mantissa, and we'd like to take
  // full advantage of that, so we need 53 bits of random source data.

  // First, start with 32 random bits, shifted to the left 21 bits.
  double result = (double)advanceState(&well) * (1 << 21);

  // Then add another 21 random bits.
  result += (double)(advanceState(&well) & ((1 << 21) - 1));

  // Now we have a number from 0 - (2^53). Divide be the range to get a double
  // from 0 to 1.0 (half-inclusive).
  result /= 9007199254740992.0;

  return result;
}

void gab_lib_between(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                     size_t argc, gab_value argv[argc]) {

  double min = 0, max = 1;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_random");

      return;
    }

    max = gab_valton(argv[1]);

    break;
  }

  case 3: {
    if (gab_valknd(argv[1]) != kGAB_NUMBER ||
        gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "Invalid call to gab_lib_random");

      return;
    }

    min = gab_valton(argv[1]);
    max = gab_valton(argv[2]);
    break;
  }

  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_random");
  }

  double num = min + (random_float() * max);

  gab_value res = gab_number(num);

  gab_vmpush(vm, res);
}

void gab_lib_floor(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                   size_t argc, gab_value argv[argc]) {

  if (argc != 1 || gab_valknd(argv[0]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_floor");

    return;
  }

  double float_num = gab_valton(argv[0]);
  int64_t int_num = gab_valton(argv[0]);

  gab_value res = gab_number(int_num + (float_num < 0));

  gab_vmpush(vm, res);
}

void gab_lib_to_n(struct gab_eg *gab, struct gab_gc *, struct gab_vm *vm,
                  size_t argc, gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_from");
    return;
  }

  s_char str = gab_valintocs(gab, argv[0]);

  gab_value res = gab_number(strtod(str.data, NULL));

  gab_vmpush(vm, res);
};

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  const char *names[] = {
      "between",
      "floor",
      "to_n",
  };

  gab_value receivers[] = {
      gab_typ(gab, kGAB_NUMBER),
      gab_typ(gab, kGAB_NUMBER),
      gab_typ(gab, kGAB_STRING),
  };

  gab_value values[] = {
      gab_sbuiltin(gab, "between", gab_lib_between),
      gab_sbuiltin(gab, "floor", gab_lib_floor),
      gab_sbuiltin(gab, "to_n", gab_lib_to_n),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(values));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));

  for (int i = 0; i < LEN_CARRAY(names); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = values[i],
                  });
  }

  gab_ngciref(gab, gc, vm, 1, LEN_CARRAY(receivers), receivers);

  return NULL;
}