#ifndef GAB_COMMON_H
#define GAB_COMMON_H
// Compilation options

// Collect as frequently as possible (on every allocation)
#define GAB_DEBUG_GC 0
// Log what is happening during collection.
#define GAB_LOG_GC 0
// Log what is happening during execution.
#define GAB_LOG_EXECUTION 0
// TODO: Concurrent garbage collection.
#define GAB_CONCURRENT_GC 0
// Use computed go to instead of a switch statement in the vm.
#define GAB_GOTO 1

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
// Test directory prefix
#define GAB_TEST_DIR "../src/tests"

// Derived macros
// Garbage collection increment/decrement buffer size
#define INC_DEC_MAX (STACK_MAX + 1)
// Size of the engines constant table.
#define MODULE_CONSTANTS_MAX (UINT16_MAX + 1)
// Maximum number of function defintions that can be nested.
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// Not configurable, just constants
// Maximum index of a local.
#define LOCAL_MAX 256
// Maximum index of an upvalues.
#define UPVALUE_MAX 256
// Maximum number of function arguments.
#define FARG_MAX 16
// Maximum number of function return values.
#define FRET_MAX 16
// Value used to encode variable return.
#define VAR_RET 255

// GAB optional flags
#define GAB_FLAG_NONE 0
#define GAB_FLAG_DUMP_BYTECODE 1

// VERSION
#define GAB_VERSION_MAJOR 0
#define GAB_VERSION_MINOR 1

#include "assert.h"
#include "memory.h"
#include "types.h"

#endif
