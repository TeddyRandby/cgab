#ifndef GAB_TYPES_H
#define GAB_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>

#if defined(_WIN32) || defined(_WIN64) || defined(WI32)
#define OS_UNIX 0
#else
#define OS_UNIX 1
#endif

#define FLEXIBLE_ARRAY

typedef bool boolean;

// ~ Unsigned ints
typedef unsigned char u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef atomic_uint_fast32_t atomic_u32;
typedef atomic_uint_fast64_t atomic_u64;

// ~ Signed ints
typedef signed char i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef atomic_int_fast32_t atomic_i32;
typedef atomic_uint_fast64_t atomic_i64;

// ~ Floating Points
typedef float f32;
typedef double f64;

#define NEW(type) ((type *)malloc(sizeof(type)))

#define NEW_FLEX(obj_type, flex_type, flex_count)                              \
  ((obj_type *)malloc(sizeof(obj_type) + sizeof(flex_type) * (flex_count)))

#define NEW_ARRAY(type, count) ((type *)malloc(sizeof(type) * count))

#define DESTROY(ptr) (free(ptr))

#endif
