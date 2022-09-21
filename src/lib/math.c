
#include "../gab/gab.h"
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

gab_value gab_lib_random(gab_engine *eng, gab_value *argv, u8 argc) {

  f64 min, max;

  switch (argc) {
  case 1: {
    if (!GAB_VAL_IS_NUMBER(argv[0])) {
      return GAB_VAL_NULL();
    }

    min = 0;
    max = GAB_VAL_TO_NUMBER(argv[0]);

    break;
  }

  case 2: {
    if (!GAB_VAL_IS_NUMBER(argv[0])) {
      return GAB_VAL_NULL();
    }

    if (!GAB_VAL_IS_NUMBER(argv[1])) {
      return GAB_VAL_NULL();
    }

    min = GAB_VAL_TO_NUMBER(argv[0]);
    max = GAB_VAL_TO_NUMBER(argv[1]);
    break;
  }

  default:
    return GAB_VAL_NULL();
  }

  f64 num = min + (random_float() * max);

  return GAB_VAL_NUMBER(num);
}

gab_value gab_lib_floor(gab_engine *eng, gab_value *argv, u8 argc) {

  if (argc != 1 || !GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  f64 float_num = GAB_VAL_TO_NUMBER(argv[0]);
  i64 int_num = GAB_VAL_TO_NUMBER(argv[0]);

  return GAB_VAL_NUMBER(int_num + (float_num < 0));
}

gab_value gab_mod(gab_engine *gab) {
  gab_lib_kvp math_kvps[] = {GAB_KVP_BUILTIN(random, 3),
                             GAB_KVP_BUILTIN(floor, 1)};

  return gab_bundle_kvps(gab, GAB_KVP_BUNDLESIZE(math_kvps), math_kvps);
}
