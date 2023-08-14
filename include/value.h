#ifndef GAB_VALUE_H
#define GAB_VALUE_H
#include "include/core.h"

// An IEEE 754 double-precision float is a 64-bit value with bits laid out like:
//
// 1 Sign bit
// | 11 Exponent bits
// | |          52 Mantissa (i.e. fraction) bits
// | |          |
// S[Exponent-][Mantissa------------------------------------------]
//
// The details of how these are used to represent numbers aren't really
// relevant here as long we don't interfere with them. The important bit is NaN.
//
// An IEEE double can represent a few magical values like NaN ("not a number"),
// Infinity, and -Infinity. A NaN is any value where all exponent bits are set:
//
//  v--NaN bits
// -11111111111----------------------------------------------------
//
// Here, "-" means "doesn't matter". Any bit sequence that matches the above is
// a NaN. With all of those "-", it obvious there are a *lot* of different
// bit patterns that all mean the same thing. NaN tagging takes advantage of
// this. We'll use those available bit patterns to represent things other than
// numbers without giving up any valid numeric values.
//
// NaN values come in two flavors: "signalling" and "quiet". The former are
// intended to halt execution, while the latter just flow through arithmetic
// operations silently. We want the latter. Quiet NaNs are indicated by setting
// the highest mantissa bit:
//
//             v--Highest mantissa bit
// -[NaN      ]1---------------------------------------------------
//
// If all of the NaN bits are set, it's not a number. Otherwise, it is.
// That leaves all of the remaining bits as available for us to play with. We
// stuff a few different kinds of things here: special singleton values like
// "true", "false", and "null", and pointers to objects allocated on the heap.
// We'll use the sign bit to distinguish singleton values from pointers. If
// it's set, it's a pointer.
//
// v--Pointer or singleton?
// S[NaN      ]1---------------------------------------------------
//
// For singleton values, we just enumerate the different values. We'll use the
// low bits of the mantissa for that, and only need a few:
//
//                                                 3 Type bits--v
// 0[NaN      ]1------------------------------------------------[T]
//
// For pointers, we are left with 51 bits of mantissa to store an address.
// That's more than enough room for a 32-bit address. Even 64-bit machines
// only actually use 48 bits for addresses, so we've got plenty. We just stuff
// the address right into the mantissa.
//
// Ta-da, double precision numbers, pointers, and a bunch of singleton values,
// all stuffed into a single 64-bit sequence. Even better, we don't have to
// do any masking or work to extract number values: they are unmodified. This
// means math on numbers is fast.

typedef uint64_t gab_value;

typedef enum gab_kind {
  kGAB_NAN = 0,
  kGAB_UNDEFINED = 1,
  kGAB_NIL = 2,
  kGAB_FALSE = 3,
  kGAB_TRUE,
  kGAB_PRIMITIVE,
  kGAB_NUMBER,
  kGAB_SUSPENSE,
  kGAB_STRING,
  kGAB_MESSAGE,
  kGAB_BLOCK_PROTO,
  kGAB_SUSPENSE_PROTO,
  kGAB_BUILTIN,
  kGAB_BLOCK,
  kGAB_BOX,
  kGAB_RECORD,
  kGAB_SHAPE,
  kGAB_NKINDS,
} gab_kind;

#define T gab_value
#include "include/array.h"

#define __GAB_QNAN ((u64)0x7ffc000000000000)

#define __GAB_SIGN_BIT ((u64)1 << 63)

#define __GAB_TAGMASK (7)

#define __GAB_VAL_TAG(val) ((u64)((val)&__GAB_TAGMASK))

static inline f64 __gab_valtod(gab_value value) { return *(f64 *)(&value); }

static inline gab_value __gab_dtoval(f64 value) {
  return *(gab_value *)(&value);
}

#define __gab_valisn(val) (((val)&__GAB_QNAN) != __GAB_QNAN)

#define gab_valiso(val)                                                        \
  (((val) & (__GAB_QNAN | __GAB_SIGN_BIT)) == (__GAB_QNAN | __GAB_SIGN_BIT))

#define __gab_obj(val)                                                         \
  (gab_value)(__GAB_SIGN_BIT | __GAB_QNAN | (u64)(uintptr_t)(val))

#define gab_nil ((gab_value)(u64)(__GAB_QNAN | kGAB_NIL))
#define gab_false ((gab_value)(u64)(__GAB_QNAN | kGAB_FALSE))
#define gab_true ((gab_value)(u64)(__GAB_QNAN | kGAB_TRUE))
#define gab_undefined ((gab_value)(u64)(__GAB_QNAN | kGAB_UNDEFINED))

#define gab_bool(val) (val ? gab_true : gab_false)
#define gab_number(val) (__gab_dtoval(val))
#define gab_primitive(op)                                                      \
  ((gab_value)(kGAB_PRIMITIVE | __GAB_QNAN | ((u64)op << 8)))

#define gab_valton(val) (__gab_valtod(val))

#define gab_valtoo(val)                                                        \
  ((gab_obj *)(uintptr_t)((val) & ~(__GAB_SIGN_BIT | __GAB_QNAN)))

#define gab_valtop(val) ((u8)((val >> 8) & 0xff))

#endif
