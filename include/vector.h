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

#define MAX(a,b) ((a) > (b) ? (a) : (b))
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

LINKAGE void METHOD(copy)(TYPENAME*self, TYPENAME*other) {
  self->cap = other->cap;
  self->len = other->len;
  self->data = NEW_ARRAY(T, other->cap);
  memcpy(self->data, other->data, other->len * sizeof(T));
}

LINKAGE void METHOD(destroy)(TYPENAME *self) { DESTROY(self->data); }

LINKAGE u64 METHOD(set)(TYPENAME *self, u64 index, T value) {
  assert(index < self->len);
  self->data[index] = value;
  return index;
}

LINKAGE u32 METHOD(push)(TYPENAME *self, T value) {
  if (self->len >= self->cap) {
    self->data = GROW(T, self->data, MAX(8, self->cap * 2));
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
    self->data = GROW(T, self->data, MAX(8, self->cap * 2));
    self->cap = self->cap * 2;
  }
  return self->data + (self->len++);
}

LINKAGE T METHOD(del)(TYPENAME *self, u64 index) {
  assert(index < self->len);
  if (index + 1 == self->len) {
      return METHOD(pop)(self);
  }
  T removed = METHOD(val_at)(self, index);
  memcpy(self->data + index, self->data + index + 1, self->len-- - index);
  return removed;
}

#undef T
#undef TYPENAME
#undef NAME
#undef GROW
#undef PREFIX
#undef LINKAGE
#undef METHOD
#undef CONCAT
#undef CONCAT_
