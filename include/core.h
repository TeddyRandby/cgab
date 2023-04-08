#ifndef GAB_COMMON_H
#define GAB_COMMON_H
// Compilation options

// Collect as frequently as possible (on every RC push). Also collect debug
// info.
#define GAB_DEBUG_GC 1
// Log what is happening during collection.
#define GAB_LOG_GC 1
// Log what is happening during execution.
#define GAB_LOG_EXECUTION 0
// Make sure functions don't break out of their frame
#define GAB_DEBUG_VM 1

// Configurable macros
// Dictionary maximum load
#define DICT_MAX_LOAD 0.6
// Maximum number of call frames that can be on the call stack
#define FRAMES_MAX 512
// Maximum number of function defintions that can be nested.
#define FUNCTION_DEF_NESTING_MAX 64
// Initial capacity of interned table
#define INTERN_INITIAL_CAP 256
// Initial capacity of module constant table
#define CONSTANTS_INITIAL_CAP 64
// Use the simple custom chunk allocator
#define CHUNK_ALLOCATOR 0

// Derived macros
// Garbage collection increment/decrement buffer size
#define GC_DEC_BUFF_MAX (STACK_MAX) // This MUST be at LEAST STACK_MAX
#define GC_INC_BUFF_MAX (STACK_MAX)
#define GC_ROOT_BUFF_MAX (256)
// Size of the engines constant table.
#define CONSTANTS_MAX (UINT16_MAX + 1)
// Maximum size of the stack
#define STACK_MAX (FRAMES_MAX * 256)

// Not configurable, just constants
// Maximum value of a local.
#define LOCAL_MAX 255
// Maximum value of an upvalues.
#define UPVALUE_MAX 255
// Maximum number of function arguments.
#define ARG_MAX 128
// Maximum number of function return values.
#define RET_MAX 128
// Values used to encode variable return.
#define VAR_EXP 255
#define FLAG_VAR_EXP 1

// GAB optional flags
#define GAB_FLAG_NONE 0
#define GAB_FLAG_DUMP_BYTECODE 1
#define GAB_FLAG_DUMP_ERROR 2
#define GAB_FLAG_EXIT_ON_PANIC 4

// VERSION
#define GAB_VERSION_MAJOR 0
#define GAB_VERSION_MINOR 1

#define GAB_MESSAGE_ADD "+"
#define GAB_MESSAGE_SUB "-"
#define GAB_MESSAGE_MUL "*"
#define GAB_MESSAGE_DIV "/"
#define GAB_MESSAGE_MOD "%"
#define GAB_MESSAGE_BND "&"
#define GAB_MESSAGE_BOR "|"
#define GAB_MESSAGE_LSH "<<"
#define GAB_MESSAGE_RSH ">>"
#define GAB_MESSAGE_LT "<"
#define GAB_MESSAGE_GT ">"
#define GAB_MESSAGE_LTE "<="
#define GAB_MESSAGE_GTE ">="
#define GAB_MESSAGE_EQ "=="
#define GAB_MESSAGE_SET "[=]"
#define GAB_MESSAGE_GET "[]"
#define GAB_MESSAGE_CAL "()"

#define LEN_CARRAY(arr) (sizeof(arr) / sizeof(arr[0]))

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

#define T s_i8
#include "array.h"

#define T u64
#include "array.h"

#define K u64
#define V u64
#define DEF_V 0
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#define LOAD DICT_MAX_LOAD
#include "dict.h"

#endif
