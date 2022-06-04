#ifndef BLUF_COMMON_H
#define BLUF_COMMON_H
// Collect as frequently as possible (on every allocation)
#define GAB_DEBUG_GC 1
// Log what is happening durin collection.
#define GAB_LOG_GC 0
// Log what is happening durin collection.
#define GAB_LOG_EXECUTION 0
// TODO: Concurrent garbage collection.
#define GAB_CONCURRENT_GC 0
// Dictionary maximum load
#define DICT_MAX_LOAD 0.75
// Garbage collection increment/decrement buffer size
#define INC_DEC_MAX (STACK_MAX + 1)
// Size of the engines constant table.
#define MODULE_CONSTANTS_MAX (UINT16_MAX + 1)
// Size of the initial capacity for objects.
#define OBJECT_INITIAL_CAP 8
// Use computed go to instead of a switch statement in the vm.
#define GAB_GOTO 1
// Maximum number of call frames that can be on the call stack
#define GAB_TEST_DIR "../src/tests"
#define FRAMES_MAX 64
// Maximum size of the stack
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
// Maximum number of function defintions that can be nested.
#define FUNCTION_DEF_NESTING_MAX 32
// Maximum number of roots inthe root buffer before triggering a collection.
#define ROOT_MAX 2048
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

#include "assert.h"
#include "io.h"
#include "memory.h"
#include "types.h"

#endif
