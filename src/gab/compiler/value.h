#ifndef GAB_VALUE_H
#define GAB_VALUE_H
#include "../../core/core.h"

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
typedef u64 gab_value;

#define QNAN ((u64)0x7ffc000000000000)

#define SIGN_BIT ((u64)1 << 63)

#define GAB_TAG_NAN (0)
#define GAB_TAG_NULL (1)
#define GAB_TAG_FALSE (2)
#define GAB_TAG_TRUE (3)
#define GAB_TAG_UNDEFINED (4)
#define GAB_TAG_UNUSED2 (5)
#define GAB_TAG_UNUSED3 (6)
#define GAB_TAG_UNUSED4 (7)

#define GAB_VAL_IS_NULL(val) (val == GAB_NULL)
#define GAB_VAL_IS_UNDEFINED(val) (val == GAB_UNDEFINED)
#define GAB_VAL_IS_FALSE(val) (val == GAB_FALSE)
#define GAB_VAL_IS_NUMBER(val) (((val)&QNAN) != QNAN)
#define GAB_VAL_IS_BOOLEAN(val) (((val) | 1) == GAB_TRUE)
#define GAB_VAL_IS_OBJ(val) (((val) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define GAB_NULL ((gab_value)(u64)(QNAN | GAB_TAG_NULL))
#define GAB_FALSE ((gab_value)(u64)(QNAN | GAB_TAG_FALSE))
#define GAB_TRUE ((gab_value)(u64)(QNAN | GAB_TAG_TRUE))
#define GAB_UNDEFINED ((gab_value)(u64)(QNAN | GAB_TAG_UNDEFINED))

#define GAB_VAL_UNDEFINED() (GAB_UNDEFINED)
#define GAB_VAL_NULL() (GAB_NULL)
#define GAB_VAL_NUMBER(val) (f64_to_value(val))
#define GAB_VAL_BOOLEAN(val) (val ? GAB_TRUE : GAB_FALSE)
#define GAB_VAL_OBJ(val) (gab_value)(SIGN_BIT | QNAN | (u64)(uintptr_t)(val))

#define GAB_VAL_TO_BOOLEAN(val) ((val) == GAB_TRUE)
#define GAB_VAL_TO_NUMBER(val) (value_to_f64(val))
#define GAB_VAL_TO_OBJ(val) ((gab_obj *)(uintptr_t)((val) & ~(SIGN_BIT | QNAN)))

#endif
