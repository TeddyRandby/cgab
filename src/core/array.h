#include "hash.h"
#include "string.h"
#include "types.h"

#ifndef T
#error "Define a type T before including this header"
#endif

#define CONCAT(a, b) CONCAT_(a, b)
#define CONCAT_(a, b) a##b

#define TYPENAME CONCAT(a_, T)
#define PREFIX TYPENAME
#define LINKAGE static inline
#define METHOD(name) CONCAT(PREFIX, CONCAT(_, name))

typedef struct TYPENAME TYPENAME;
struct TYPENAME {
  u64 len;
  T data[FLEXIBLE_ARRAY];
};

LINKAGE TYPENAME *METHOD(create)(T *data, u64 len) {
  TYPENAME *self = NEW_FLEX(TYPENAME, T, len);
  memcpy(self->data, data, len * sizeof(T));
  self->len = len;
  return self;
}

LINKAGE TYPENAME *METHOD(empty)(u64 len) {
  TYPENAME *self = NEW_FLEX(TYPENAME, T, len);
  memset(self->data, 0, len * sizeof(T));
  self->len = len;
  return self;
}

LINKAGE boolean METHOD(match)(TYPENAME *self, TYPENAME *other) {
  if (self->len != other->len)
    return false;
  return memcmp(self->data, other->data, self->len) == 0;
}

LINKAGE u64 METHOD(hash)(TYPENAME *self) {
  return hash_bytes(self->len * sizeof(T), (u8 *)self->data);
}

LINKAGE void METHOD(destroy)(TYPENAME *self) { DESTROY(self); }

#undef T
#undef TYPENAME
#undef PREFIX
#undef LINKAGE
#undef METHOD
#undef CONCAT
#undef CONCAT_
