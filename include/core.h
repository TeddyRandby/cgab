#ifndef GAB_COMMON_H
#define GAB_COMMON_H

// Collect as frequently as possible (on every RC push) and collect debug info.
#ifndef cGAB_DEBUG_GC
#define cGAB_DEBUG_GC 0
#endif

// Log what is happening during collection.
#ifndef cGAB_LOG_GC
#define cGAB_LOG_GC 0
#endif

// Log what is happening during execution.
#ifndef cGAB_DEBUG_VM
#define cGAB_DEBUG_VM 0
#endif

// Make sure functions don't break out of their frame
#ifndef cGAB_LOG_VM
#define cGAB_LOG_VM 0
#endif

// Use the simple custom chunk allocator
#ifndef cGAB_CHUNK_ALLOCATOR
#define cGAB_CHUNK_ALLOCATOR 1
#endif

// Capacity at which point dictionaries are resized
#ifndef cGAB_DICT_MAX_LOAD
#define cGAB_DICT_MAX_LOAD 0.6
#endif

// Maximum number of call frames that can be on the call stack
#ifndef cGAB_FRAMES_MAX
#define cGAB_FRAMES_MAX 512
#endif

// Maximum number of function defintions that can be nested.
#ifndef cGAB_FUNCTION_DEF_NESTING_MAX
#define cGAB_FUNCTION_DEF_NESTING_MAX 64
#endif

// Initial capacity of interned table
#ifndef cGAB_INTERN_INITIAL_CAP
#define cGAB_INTERN_INITIAL_CAP 256
#endif

// Initial capacity of module constant table
#ifndef cGAB_CONSTANTS_INITIAL_CAP
#define cGAB_CONSTANTS_INITIAL_CAP 64
#endif

// Size of the vm's stack
#ifndef cGAB_STACK_MAX
#define cGAB_STACK_MAX (cGAB_FRAMES_MAX * 256)
#endif

// Garbage collection increment/decrement buffer size
#ifndef cGAB_GC_DEC_BUFF_MAX
#define cGAB_GC_DEC_BUFF_MAX (cGAB_STACK_MAX)
#endif

#if cGAB_GC_DEC_BUFF_MAX < STACK_MAX
#error "cGAB_GC_DEC_BUFF_MAX must be less than or equal to STACK_MAX"
#endif

#ifndef cGAB_GC_INC_BUFF_MAX
#define cGAB_GC_INC_BUFF_MAX (cGAB_STACK_MAX)
#endif

#if cGAB_GC_DEC_BUFF_MAX < STACK_MAX
#error "cGAB_GC_DEC_BUFF_MAX must be less than or equal to STACK_MAX"
#endif

#ifndef cGAB_GC_ROOT_BUFF_MAX
#define cGAB_GC_ROOT_BUFF_MAX (256)
#endif

// Not configurable, just constants
#define GAB_CONSTANTS_MAX (UINT16_MAX + 1)
// Maximum value of a local.
#define GAB_LOCAL_MAX 255
// Maximum value of an upvalues.
#define GAB_UPVALUE_MAX 255
// Maximum number of function arguments.
#define GAB_ARG_MAX 128
// Maximum number of function return values.
#define GAB_RET_MAX 128

#define VAR_EXP 255
#define FLAG_VAR_EXP 1

// GAB optional flags
#define fGAB_DUMP_BYTECODE (1 << 0)
#define fGAB_DUMP_ERROR (1 << 1)
#define fGAB_EXIT_ON_PANIC (1 << 2)
#define fGAB_STREAM_INPUT (1 << 3)
#define fGAB_DELIMIT_INPUT (1 << 4)

// VERSION
#define GAB_VERSION_MAJOR 0
#define GAB_VERSION_MINOR 1

// Message constants
#define mGAB_ADD "+"
#define mGAB_SUB "-"
#define mGAB_MUL "*"
#define mGAB_DIV "/"
#define mGAB_MOD "%"
#define mGAB_BND "&"
#define mGAB_BOR "|"
#define mGAB_LSH "<<"
#define mGAB_RSH ">>"
#define mGAB_LT "<"
#define mGAB_GT ">"
#define mGAB_LTE "<="
#define mGAB_GTE ">="
#define mGAB_EQ "=="
#define mGAB_SET "[=]"
#define mGAB_GET "[]"
#define mGAB_CALL "()"

#include "types.h"

#define T i8
#include "slice.h"

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

#define T i8
#include "vector.h"

#define T s_i8
#include "array.h"

#define T a_i8 *
#define NAME a_i8
#include "vector.h"

#define T u64
#include "array.h"

#define K u64
#define V u64
#define DEF_V 0
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

static inline s_i8 s_i8_cstr(const char *str) {
  return (s_i8){.data = (i8 *)str, .len = strlen(str)};
}

static inline s_i8 s_i8_arr(const a_i8 *str) {
  return (s_i8){.data = str->data, .len = str->len};
}

static inline s_i8 s_i8_tok(s_i8 str, u64 start, i8 ch) {
  if (start >= str.len)
    return (s_i8){.data = str.data + start, .len = 0};

  u64 cursor = start;

  while (cursor < str.len && str.data[cursor] != ch)
    cursor++;

  return (s_i8){.data = str.data + start, .len = cursor - start};
}

#define LEN_CARRAY(a) (sizeof(a) / sizeof(a[0]))

#endif
