#include "types.h"
#include <assert.h>
#include <string.h>

#ifndef K
#error "Define a type K before including this header"
#endif

#ifndef HASH
#error "Define a hash function HASH before including this header"
#endif

#ifndef EQUAL
#error "Define an equal function EQUAL before including this header"
#endif

#ifndef V
#define V u8
#endif

#ifndef NAME
#define NAME K
#endif

#ifndef LOAD
#define LOAD 0.75
#endif

#define CONCAT(a, b) CONCAT_(a, b)
#define CONCAT_(a, b) a##b

#define TYPENAME CONCAT(d_, NAME)
#define BUCKET_T CONCAT(TYPENAME, _BUCKET)
#define PREFIX TYPENAME
#define LINKAGE static inline
#define METHOD(name) CONCAT(PREFIX, CONCAT(_, name))

#ifndef CORE_DICT_H
#define CORE_DICT_H

typedef enum d_status {
  D_EMPTY = 0,
  D_FULL = 1,
  D_TOMBSTONE = 2,
} d_status;

#endif

typedef struct BUCKET_T BUCKET_T;
struct BUCKET_T {
  K key;
  V val;
  d_status status;
};

typedef struct TYPENAME TYPENAME;
struct TYPENAME {
  u64 len;
  u64 cap;
  BUCKET_T *buckets;
};

LINKAGE K METHOD(ikey)(TYPENAME *self, u64 index) {
  return self->buckets[index].key;
}

LINKAGE V METHOD(ival)(TYPENAME *self, u64 index) {
  return self->buckets[index].val;
}

LINKAGE void METHOD(iset_key)(TYPENAME *self, u64 index, K key) {
  assert(index < self->cap);
  self->buckets[index].key = key;
}

LINKAGE void METHOD(iset_val)(TYPENAME *self, u64 index, V val) {
  assert(index < self->cap);
  self->buckets[index].val = val;
}

LINKAGE boolean METHOD(iexists)(TYPENAME *self, u64 index) {
  return self->buckets[index].status == D_FULL;
}

LINKAGE d_status METHOD(istatus)(TYPENAME *self, u64 index) {
  return self->buckets[index].status;
}

LINKAGE boolean METHOD(iremove)(TYPENAME *self, u64 index) {
  if (self->len == 0)
    return false;

  BUCKET_T *bucket = self->buckets + index;
  if (bucket->status != D_FULL)
    return false;

  bucket->status = D_TOMBSTONE;
  return true;
}

LINKAGE u64 METHOD(next)(TYPENAME *self, u64 index) {
  if (index >= self->cap)
    return -1;

  while (!METHOD(iexists)(self, index)) {
    index++;
    if (index == self->cap)
      return -1;
  }

  return index;
}

LINKAGE void METHOD(create)(TYPENAME *self, u64 cap) {
  assert(cap % 2 == 0);

  self->buckets = NEW_ARRAY(BUCKET_T, cap);
  memset(self->buckets, 0, cap * sizeof(BUCKET_T));

  self->cap = cap;
  self->len = 0;
}

LINKAGE void METHOD(destroy)(TYPENAME *self) { DESTROY(self->buckets); }

LINKAGE u64 METHOD(index_of)(TYPENAME *self, K key) {
  u64 index = HASH(key) & (self->cap - 1);
  i64 tombstone = -1;

  for (;;) {
    BUCKET_T *bucket = self->buckets + index;
    K test_key = bucket->key;

    switch (bucket->status) {
    case D_TOMBSTONE:
      if (tombstone == -1)
        tombstone = index;
      break;
    case D_EMPTY:
      return tombstone == -1 ? index : tombstone;
    case D_FULL:
      if (EQUAL(test_key, key))
        return index;
    }

    index = (index + 1) & (self->cap - 1);
  }
}

LINKAGE void METHOD(cap)(TYPENAME *self, u64 cap) {
  TYPENAME other;
  METHOD(create)(&other, cap);

  self->len = 0;
  for (u64 i = 0; i < self->cap; i++) {
    BUCKET_T *bucket = self->buckets + i;

    if (!(bucket->status == D_FULL))
      continue;

    u64 index = METHOD(index_of)(&other, bucket->key);
    BUCKET_T *other_bucket = other.buckets + index;

    other_bucket->key = bucket->key;
    other_bucket->val = bucket->val;
    other_bucket->status = bucket->status;

    self->len++;
  }

  DESTROY(self->buckets);
  self->buckets = other.buckets;
  self->cap = cap;
}

LINKAGE boolean METHOD(insert)(TYPENAME *self, K key, V val) {
  if (self->len + 1 > (self->cap * LOAD)) {
    METHOD(cap)(self, self->cap * 2);
  }

  u64 index = METHOD(index_of)(self, key);

  BUCKET_T *bucket = self->buckets + index;

  boolean is_new = bucket->status == D_EMPTY;

  if (is_new)
    self->len++;

  bucket->key = key;
  bucket->val = val;
  bucket->status = D_FULL;

  return is_new;
}

LINKAGE boolean METHOD(remove)(TYPENAME *self, K key) {
  if (self->len == 0)
    return false;

  u64 index = METHOD(index_of)(self, key);

  BUCKET_T *bucket = self->buckets + index;

  if (!(bucket->status == D_FULL))
    return false;

  bucket->status = D_TOMBSTONE;
  return true;
}

LINKAGE boolean METHOD(exists)(TYPENAME *self, K key) {
  if (self->len == 0) {
    return false;
  }

  u64 index = METHOD(index_of)(self, key);

  BUCKET_T *bucket = self->buckets + index;

  return bucket->status == D_FULL;
}

LINKAGE V METHOD(read)(TYPENAME *self, K key) {
  u64 index = METHOD(index_of)(self, key);

  BUCKET_T *bucket = self->buckets + index;

  return bucket->val;
}

#undef NAME
#undef K
#undef V
#undef HASH
#undef LOAD
#undef EQUAL
#undef TYPENAME
#undef PREFIX
#undef LINKAGE
#undef METHOD
#undef CONCAT
#undef CONCAT_
