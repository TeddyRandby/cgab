#ifndef GAB_TYPES_H
#define GAB_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>

#if defined(_WIN32) || defined(_WIN64) || defined(Wint32_t)
#define OS_UNIX 0
#else
#define OS_UNIX 1
#endif

#define FLEXIBLE_ARRAY

#define NEW(type) ((type *)malloc(sizeof(type)))

#define NEW_FLEX(obj_type, flex_type, flex_count)                              \
  ((obj_type *)malloc(sizeof(obj_type) + sizeof(flex_type) * (flex_count)))

#define NEW_ARRAY(type, count) ((type *)malloc(sizeof(type) * count))

#define DESTROY(ptr) (free(ptr))

#endif
