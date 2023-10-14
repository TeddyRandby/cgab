#ifndef GAB_COMMON_H
#define GAB_COMMON_H

// Collect as frequently as possible (on every RC push) and collect debug info.
#ifndef cGAB_DEBUG_GC
#define cGAB_DEBUG_GC 0
#endif

// Log what is happening during collection.
#ifndef cGAB_LOG_GC
#define cGAB_LOG_GC 1
#endif

// Make sure functions don't break out of their frame
#ifndef cGAB_DEBUG_VM
#define cGAB_DEBUG_VM 0
#endif

#ifndef cGAB_DEBUG_BC
#define cGAB_DEBUG_BC 0
#endif

// Log what is happening during execution.
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
#define cGAB_FRAMES_MAX 64
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
#define cGAB_STACK_MAX (cGAB_FRAMES_MAX * 128)
#endif

// Garbage collection increment/decrement buffer size
#ifndef cGAB_GC_DEC_BUFF_MAX
#define cGAB_GC_DEC_BUFF_MAX (cGAB_STACK_MAX)
#endif

#if cGAB_GC_DEC_BUFF_MAX < STACK_MAX
#error "cGAB_GC_DEC_BUFF_MAX must be less than or equal to STACK_MAX"
#endif

#ifndef cGAB_GC_MOD_BUFF_MAX
#define cGAB_GC_MOD_BUFF_MAX (cGAB_STACK_MAX)
#endif

#if cGAB_GC_DEC_BUFF_MAX < STACK_MAX
#error "cGAB_GC_DEC_BUFF_MAX must be less than or equal to STACK_MAX"
#endif

#ifndef cGAB_GC_ROOT_BUFF_MAX
#define cGAB_GC_ROOT_BUFF_MAX (32)
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

enum gab_status {
#define STATUS(name, message) GAB_##name,
#include "status_code.h"
#undef STATUS
};

// GAB optional flags
enum gab_flags {
  fGAB_DUMP_BYTECODE = 1 << 0,
  fGAB_DUMP_ERROR = 1 << 1,
  fGAB_EXIT_ON_PANIC = 1 << 2,
  fGAB_STREAM_INPUT = 1 << 3,
  fGAB_DELIMIT_INPUT = 1 << 4,
};

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

#define T char
#include "slice.h"

#define T char
#include "array.h"

#define T uint8_t
#include "vector.h"

#define T uint32_t
#include "vector.h"

#define T uint64_t
#include "vector.h"

#define T s_char
#include "vector.h"

#define T int8_t
#include "vector.h"

#define T s_char
#include "array.h"

#define T a_char *
#define NAME a_char
#include "vector.h"

#define T uint64_t
#include "array.h"

#define K uint64_t
#define V uint64_t
#define DEF_V 0
#define HASH(a) a
#define EQUAL(a, b) (a == b)
#define LOAD cGAB_DICT_MAX_LOAD
#include "dict.h"

static inline s_char s_char_cstr(const char *str) {
  return (s_char){.data = str, .len = strlen(str)};
}

static inline s_char s_char_arr(const a_char *str) {
  return (s_char){.data = str->data, .len = str->len};
}

static inline s_char s_char_tok(s_char str, uint64_t start, char ch) {
  if (start >= str.len)
    return (s_char){.data = str.data + start, .len = 0};

  uint64_t cursor = start;

  while (cursor < str.len && str.data[cursor] != ch)
    cursor++;

  return (s_char){.data = str.data + start, .len = cursor - start};
}

#define LEN_CARRAY(a) (sizeof(a) / sizeof(a[0]))

#endif
