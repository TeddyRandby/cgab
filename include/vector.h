#include "assert.h"
#include "string.h"
#include "types.h"

#ifndef T
#error "Define a type T before including this header"
#endif

#define CONCAT(a, b) CONCAT_(a, b)
#define CONCAT_(a, b) a##b

#ifndef NAME
#define TYPENAME CONCAT(v_, T)
#else
#define TYPENAME CONCAT(v_, NAME)
#endif

#define PREFIX TYPENAME
#define LINKAGE static inline
#define METHOD(name) CONCAT(PREFIX, CONCAT(_, name))

#define GROW(type, loc, new_count)                                             \
  ((type *)realloc(loc, sizeof(type) * (new_count)))

typedef struct TYPENAME TYPENAME;
struct TYPENAME {
  T *data;
  u64 len;
  u64 cap;
};

LINKAGE void METHOD(create)(TYPENAME *self, u64 cap) {
  self->cap = cap;
  self->len = 0;
  self->data = NEW_ARRAY(T, cap);
}

LINKAGE void METHOD(destroy)(TYPENAME *self) { DESTROY(self->data); }

LINKAGE u64 METHOD(set)(TYPENAME *self, u64 index, T value) {
  assert(index < self->len);
  self->data[index] = value;
  return index;
}

LINKAGE u32 METHOD(push)(TYPENAME *self, T value) {
  if (self->len >= self->cap) {
    self->data = GROW(T, self->data, self->cap * 2);
    self->cap = self->cap * 2;
  }

  return METHOD(set)(self, self->len++, value);
}

LINKAGE T METHOD(pop)(TYPENAME *self) {
  assert(self->len > 0);
  return self->data[--self->len];
}

LINKAGE T *METHOD(ref_at)(TYPENAME *self, u64 index) {
  assert(index < self->len);
  return self->data + index;
}

LINKAGE T METHOD(val_at)(TYPENAME *self, u64 index) {
  assert(index < self->len);
  return self->data[index];
}

LINKAGE T *METHOD(emplace)(TYPENAME *self) {
  if (self->len >= self->cap) {
    self->data = GROW(T, self->data, self->cap * 2);
    self->cap = self->cap * 2;
  }
  return self->data + (self->len++);
}

#undef T
#undef TYPENAME
#undef GROW
#undef PREFIX
#undef LINKAGE
#undef METHOD
#undef CONCAT
#undef CONCAT_
