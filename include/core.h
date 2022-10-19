#ifndef GAB_COMMON_H
#define GAB_COMMON_H
// Compilation options

// Collect as frequently as possible (on every allocation)
#define GAB_DEBUG_GC 1
// Log what is happening during collection.
#define GAB_LOG_GC 0
// Log what is happening during execution.
#define GAB_LOG_EXECUTION 0
// TODO: Concurrent garbage collection.
#define GAB_CONCURRENT_GC 0

// Configurable macros
// Dictionary maximum load
#define DICT_MAX_LOAD 0.6
// Size of the initial capacity for objects.
#define OBJECT_INITIAL_CAP 8
// Maximum number of call frames that can be on the call stack
#define FRAMES_MAX 64
// Maximum size of the stack
#define FUNCTION_DEF_NESTING_MAX 64
// Maximum number of roots inthe root buffer before triggering a collection.
#define ROOT_MAX 1024

// Derived macros
// Garbage collection increment/decrement buffer size
#define INC_DEC_MAX (STACK_MAX + 1)
// Size of the engines constant table.
#define MODULE_CONSTANTS_MAX (UINT16_MAX + 1)
// Maximum number of function defintions that can be nested.
#define STACK_MAX (FRAMES_MAX * 256)

// Not configurable, just constants
// Maximum value of a local.
#define LOCAL_MAX 255
// Maximum value of an upvalues.
#define UPVALUE_MAX 255
// Maximum number of function arguments.
#define FARG_MAX 16
// Maximum number of function return values.
#define FRET_MAX 16
// Value used to encode variable return.
#define VAR_RET 255

// GAB optional flags
#define GAB_FLAG_NONE 0
#define GAB_FLAG_DUMP_BYTECODE 1
#define GAB_FLAG_DUMP_ERROR 2

// VERSION
#define GAB_VERSION_MAJOR 0
#define GAB_VERSION_MINOR 1

#include "types.h"

#define T i8
#include "slice.h"

static inline s_i8 s_i8_cstr(const char *str) {
  return (s_i8){.data = (i8 *)str, .len = strlen(str)};
}

#define T i8
#include "array.h"

#define T u8
#include "vector.h"

#define T u32
#include "vector.h"

#define T u64
#include "vector.h"

#define T s_i8
#include "vector.h"

#define K u64
#define V u64
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "dict.h"

#endif
