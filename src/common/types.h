#ifndef BLUF_TYPES_H
#define BLUF_TYPES_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64) || defined(WI32)
#define OS_UNIX 0
#else
#define OS_UNIX 1
#endif

#define FLEXIBLE_ARRAY

#define UINT8_COUNT (UINT8_MAX + 1)
typedef bool boolean;

// ~ Unsigned ints
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// ~ Signed ints
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// ~ Floating Points
typedef float f32;
typedef double f64;

typedef u64 gab_value;

// ~ Assert Helpers
void failure(const char *file, const u32 line, const char *msg);

#ifdef NDEBUG
#define ASSERT_TRUE(expr, msg)
#else
#define ASSERT_TRUE(expr, msg)                                                 \
  if (!(expr))                                                                 \
  failure(__FILE__, __LINE__, msg)
#endif

// ~ Memory helpers
void copy(void *dest, const void *src, u64 size);
void move(void *dest, const void *src, u64 size);
void destroy(void *loc);

#define GAB_CREATE_ARRAY(type, count, gc)                                      \
  ((type *)gab_gc_reallocate(gc, NULL, 0, sizeof(type) * count))
#define CREATE_ARRAY(type, count) (malloc(sizeof(type) * count))

#define GAB_CREATE_STRUCT(obj_type, gc)                                        \
  ((obj_type *)gab_gc_reallocate(gc, NULL, 0, sizeof(obj_type)))

#define CREATE_STRUCT(type) (malloc(sizeof(type)))

#define GAB_CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count, gc)            \
  ((obj_type *)gab_gc_reallocate(                                              \
      gc, NULL, 0, sizeof(obj_type) + sizeof(flex_type) * flex_count))

#define CREATE_FLEX_STRUCT(obj_type, flex_type, flex_count)                    \
  (malloc(sizeof(obj_type) + sizeof(flex_type) * (flex_count)))

#define GAB_DESTROY_ARRAY(type, loc, count, gc)                                \
  (gab_gc_reallocate(gc, loc, sizeof(type) * count, 0))
#define DESTROY_ARRAY(arr) (free(arr))

#define GAB_DESTROY_STRUCT(ptr, gc) gab_gc_reallocate(gc, ptr, 0, 0)
#define DESTROY_STRUCT(ptr) (free(ptr))

#define COPY(dest, src, size) copy(dest, src, size);

#define GROW(type, loc, old_count, new_count)                                  \
  (type *)realloc(loc, sizeof(type) * (new_count))

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

// ~ String type

/*
  ~ 8bit-char string ref

No destructor because
this string type is not owning.
This string is entirely on the stack.
*/
typedef struct s_u8 s_u8;

typedef struct s_u8_ref {
  u64 size;
  u8 *data;
} s_u8_ref;
s_u8_ref s_u8_ref_create_cstr(const char *data);
s_u8_ref s_u8_ref_create_s_u8(s_u8 *data);
s_u8_ref s_u8_ref_create(u8 *src, u64 size);
boolean s_u8_ref_match(s_u8_ref a, s_u8_ref b);
boolean s_u8_ref_match_lit(s_u8_ref a, const char *b);

/*
  ~ 8bit-char string.

This string is immutable - using FAM.
Also entirely on the heap.

*/
struct s_u8 {
  u64 size;
  u8 data[FLEXIBLE_ARRAY];
};
s_u8 *s_u8_create_cstr(const char *src);
s_u8 *s_u8_create_sref(const s_u8_ref other);
s_u8 *s_u8_create_empty(u64 size);
boolean s_u8_match_ref(s_u8 *self, s_u8_ref other);
boolean s_u8_match_cstr(s_u8 *self, const char *other);
void s_u8_copy_ref(s_u8 *self, s_u8_ref str);
s_u8 *s_u8_append_s_u8(s_u8 *self, s_u8 *other, boolean free_self);
void s_u8_destroy(s_u8 *self);

// ~ 8bit vector.
// owns its data and
// needs to be freed.
typedef struct v_u8 {
  u64 capacity;
  u64 size;
  u8 *data;
} v_u8;
void v_u8_create(v_u8 *self);
void v_u8_destroy(v_u8 *self);

static inline void v_u8_set(v_u8 *self, u64 index, u8 value) {
  ASSERT_TRUE(index < self->size, "Cannot set outside vector bounds");
  self->data[index] = value;
};

static inline u64 v_u8_push(v_u8 *self, u8 value) {
  if (self->size == self->capacity) {
    self->data = GROW(u8, self->data, self->capacity, self->capacity * 2);
    self->capacity *= 2;
  }

  v_u8_set(self, self->size++, value);
  return self->size - 1;
};

static inline u8 v_u8_pop(v_u8 *self) {
  ASSERT_TRUE(self->size > 0, "Cannot pop, no elements exist");
  return self->data[self->size--];
};

static inline u8 *v_u8_ref_at(v_u8 *self, u64 index) {
  ASSERT_TRUE(index < self->size, "Cannot get a ref outside vector bounds");
  return self->data + index;
};

static inline u8 v_u8_val_at(v_u8 *self, u64 index) {
  ASSERT_TRUE(index < self->size, "Cannot get a val outside vector bounds");
  return self->data[index];
};

// ~ string vector.
// owns its data and
// needs to be freed.
typedef struct v_s_u8_ref {
  u64 capacity;
  u64 size;
  s_u8_ref *data;
} v_s_u8_ref;
void v_s_u8_ref_create(v_s_u8_ref *self);
void v_s_u8_ref_destroy(v_s_u8_ref *self);

static inline void v_s_u8_ref_set(v_s_u8_ref *self, u64 index, s_u8_ref value) {
  ASSERT_TRUE(index < self->size, "Cannot set outside vector bounds");
  self->data[index] = value;
}

static inline u64 v_s_u8_ref_push(v_s_u8_ref *self, s_u8_ref value) {
  if (self->size == self->capacity) {
    self->data = GROW(s_u8_ref, self->data, self->capacity, self->capacity * 2);
    self->capacity *= 2;
  }

  v_s_u8_ref_set(self, self->size++, value);
  return self->size - 1;
};

static inline s_u8_ref v_s_u8_ref_pop(v_s_u8_ref *self) {
  ASSERT_TRUE(self->size > 0, "Cannot pop, no elements exist");
  return self->data[self->size--];
};

static inline s_u8_ref *v_s_u8_ref_ref_at(v_s_u8_ref *self, u64 index) {
  ASSERT_TRUE(index < self->size, "Cannot get a ref outside vector bounds");
  return self->data + index;
};

static inline s_u8_ref v_s_u8_ref_val_at(v_s_u8_ref *self, u64 index) {
  ASSERT_TRUE(index < self->size, "Cannot get a val outside vector bounds");
  return self->data[index];
};

// ~ 64 bit vector.
// owns its data and
// needs to be freed.
typedef struct v_u64 {
  u64 capacity;
  u64 size;
  u64 *data;
} v_u64;
void v_u64_create(v_u64 *self);
void v_u64_destroy(v_u64 *self);

static inline void v_u64_set(v_u64 *self, u64 index, u64 value) {
  ASSERT_TRUE(index < self->capacity, "Cannot set outside vector bounds");
  self->data[index] = value;
};

static inline u64 v_u64_push(v_u64 *self, u64 value) {
  if (self->size == self->capacity) {
    self->data = GROW(u64, self->data, self->capacity, self->capacity * 2);
    self->capacity *= 2;
  }

  v_u64_set(self, self->size++, value);
  return self->size - 1;
};

static inline u64 v_u64_pop(v_u64 *self) {
  ASSERT_TRUE(self->size > 0, "Cannot pop, no elements exist");
  return self->data[self->size--];
};

static inline u64 *v_u64_ref_at(v_u64 *self, u64 index) {
  ASSERT_TRUE(index < self->capacity, "Cannot get a ref outside vector bounds");
  return self->data + index;
};

static inline u64 v_u64_val_at(v_u64 *self, u64 index) {
  ASSERT_TRUE(index < self->capacity, "Cannot get a val outside vector bounds");
  return self->data[index];
};

static inline void v_u64_adjust_cap(v_u64 *self, u64 new_capacity) {
  ASSERT_TRUE(self->size < new_capacity, "Cannot shrink vector");
  self->data = GROW(u64, self->data, self->capacity, new_capacity);
  self->capacity = new_capacity;
};

#define V_U64_FOR_INDEX_DO(v, i) for (u64 i = 0; i < v->size; i++)

// ~ 64 bit map

typedef struct d_u64 {
  u64 size;
  u64 capacity;
  v_u64 keys;
  v_u64 values;
} d_u64;

void d_u64_create(d_u64 *self, u64 initial_capacity);
void d_u64_destroy(d_u64 *self);

u64 d_u64_read(d_u64 *self, u64 hash, u64 key);
boolean d_u64_insert(d_u64 *self, u64 key, u64 value, u64 hash);
boolean d_u64_remove(d_u64 *self, u64 key, u64 hash);
boolean d_u64_has_key(d_u64 *self, u64 key, u64 hash);
u64 d_u64_index_of(d_u64 *self, u64 key, u64 hash);

static inline u64 d_u64_index_key(d_u64 *self, u64 index) {
  return v_u64_val_at(&self->keys, index);
}

static inline u64 d_u64_index_value(d_u64 *self, u64 index) {
  return v_u64_val_at(&self->values, index);
}

static inline void d_u64_index_set_key(d_u64 *self, u64 index, u64 value) {
  v_u64_set(&self->keys, index, value);
}

static inline void d_u64_index_set_value(d_u64 *self, u64 index, u64 value) {
  v_u64_set(&self->values, index, value);
}

#include "../gab/compiler/nanbox.h"
static inline boolean d_u64_index_remove(d_u64 *self, u64 index) {
  if (self->size == 0)
    return false;

  u64 *found_key = v_u64_ref_at(&self->keys, index);
  u64 *found_val = v_u64_ref_at(&self->values, index);

  if (GAB_VAL_IS_UNDEFINED(*found_key))
    return false;

  *found_key = GAB_VAL_UNDEFINED();
  *found_val = GAB_VAL_BOOLEAN(true);
  return true;
}

static inline boolean d_u64_index_has_key(d_u64 *self, u64 index) {
  return !GAB_VAL_IS_UNDEFINED(d_u64_index_key(self, index));
}

static inline u64 d_u64_next_index(d_u64 *self, u64 index) {
  if (index >= self->capacity)
    return -1;

  while (!d_u64_index_has_key(self, index)) {
    index++;
    if (index == self->capacity)
      return -1;
  }

  return index;
}

#define D_U64_FOR_INDEX_DO(d, i) for (u64 i = 0; i < d.capacity; i++)
#define D_U64_FOR_INDEX_WITH_KEY_DO(d, i)                                      \
  for (u64 i = d_u64_next_index(d, 0); i != -1; i = d_u64_next_index(d, i + 1))

/*
  The result type returned by the compiler and vm.
*/

typedef enum gab_result_kind {
  RESULT_COMPILE_FAIL,
  RESULT_COMPILE_SUCCESS,
  RESULT_RUN_FAIL,
  RESULT_RUN_SUCCESS,
} gab_result_kind;

typedef u64 gab_value;
typedef struct gab_vm gab_vm;
typedef struct gab_engine gab_engine;
typedef struct gab_module gab_module;
typedef struct gab_obj_function gab_obj_function;
typedef struct gab_compiler gab_compiler;

typedef struct gab_result {
  gab_result_kind type;
  union {
    /* Successful compile */
    gab_obj_function *func;
    /* Successful run*/
    gab_value result;

    struct {
      gab_compiler *compiler;
      const char *msg;
    } compile_fail;

    struct {
      gab_engine *eng;
      gab_vm *vm;
      const char *msg;
    } run_fail;
  } as;
} gab_result;

boolean gab_result_has_error(gab_result *self);
void gab_result_dump_error(gab_result *self);

gab_result *gab_compile_fail(gab_compiler *self, const char *msg);
gab_result *gab_compile_success(gab_engine *eng, gab_module *module);

gab_result *gab_run_fail(gab_engine *eng, const char *msg);
gab_result *gab_run_success(gab_value data);

void gab_result_destroy(gab_result *self);

#endif
