#include "common.h"
#include "types.h"

static inline u64 find_index(v_u64 *keys, v_u64 *values, u64 capacity, u64 key,
                             u64 hashed) {
  u64 index = hashed & (capacity - 1);
  // IDK if this works as an invalid value.
  i64 tombstone = -1;

  for (;;) {
    u64 test_key = v_u64_val_at(keys, index);
    u64 test_val = v_u64_val_at(values, index);

    if (GAB_VAL_IS_UNDEFINED(test_key)) {
      if (GAB_VAL_IS_NULL(test_val)) {
        return tombstone != -1 ? tombstone : index;
      }

      if (tombstone == -1)
        tombstone = index;

    } else if (test_key == key) {
      return index;
    }

    index = (index + 1) & (capacity - 1);
  }
}

void adjust_capacity(d_u64 *self, u64 capacity) {
  v_u64 keys;
  v_u64 values;

  v_u64_create(&keys);
  v_u64_create(&values);

  v_u64_adjust_cap(&keys, capacity);
  v_u64_adjust_cap(&values, capacity);

  for (u64 i = 0; i < capacity; i++) {
    v_u64_set(&keys, i, GAB_VAL_UNDEFINED());
    v_u64_set(&values, i, GAB_VAL_NULL());
  }

  self->size = 0;
  for (u64 i = 0; i < self->capacity; i++) {

    u64 key = v_u64_val_at(&self->keys, i);
    u64 value = v_u64_val_at(&self->values, i);

    if (GAB_VAL_IS_UNDEFINED(key))
      continue;

    // This assumes the key is identical to the hash.
    // This is fine for normal gab_values, because its true
    // This is not okay for interning strings.
    // But that only happens in the constants module,
    // Which should never have its capacity adjusted anyway.
    u64 index = find_index(&keys, &values, capacity, key, key);

    v_u64_set(&keys, index, key);
    v_u64_set(&values, index, value);
    self->size++;
  }

  v_u64_destroy(&self->keys);
  v_u64_destroy(&self->values);

  self->keys = keys;
  self->values = values;
  self->capacity = capacity;
}

void d_u64_create(d_u64 *self, u64 initial_capacity) {
  ASSERT_TRUE(initial_capacity % 2 == 0,
              "Dictionary capacity must be power of 2");

  self->size = 0;
  self->capacity = initial_capacity;
  // Initialize with a big capacity.
  // Do this so that no reallocations are made
  // because module uses a dict for its contants,
  // but indexes at runtime with the offset into the entries.
  v_u64_create(&self->keys);
  v_u64_create(&self->values);

  v_u64_adjust_cap(&self->keys, self->capacity);
  v_u64_adjust_cap(&self->values, self->capacity);

  for (u64 i = 0; i < self->capacity; i++) {
    v_u64_set(&self->keys, i, GAB_VAL_UNDEFINED());
    v_u64_set(&self->values, i, GAB_VAL_NULL());
  }
}

void d_u64_destroy(d_u64 *self) {
  self->size = 0;
  self->capacity = 0;
  v_u64_destroy(&self->keys);
  v_u64_destroy(&self->values);
}

boolean d_u64_insert(d_u64 *self, u64 key, u64 value, u64 hash) {
  if (self->size + 1 > (self->capacity * DICT_MAX_LOAD)) {
    adjust_capacity(self, GROW_CAPACITY(self->capacity));
  }

  u64 index = find_index(&self->keys, &self->values, self->capacity, key, hash);

  u64 *found_key = v_u64_ref_at(&self->keys, index);
  u64 *found_val = v_u64_ref_at(&self->values, index);

  boolean is_new = GAB_VAL_IS_UNDEFINED(*found_key);

  if (is_new && GAB_VAL_IS_NULL(*found_val))
    self->size++;

  *found_key = key;
  *found_val = value;
  return is_new;
}

boolean d_u64_remove(d_u64 *self, u64 key, u64 hash) {
  if (self->size == 0)
    return false;

  u64 index = find_index(&self->keys, &self->values, self->capacity, key, hash);

  u64 *found_key = v_u64_ref_at(&self->keys, index);
  u64 *found_val = v_u64_ref_at(&self->values, index);

  if (GAB_VAL_IS_UNDEFINED(*found_key))
    return false;

  *found_key = GAB_VAL_UNDEFINED();
  *found_val = GAB_VAL_BOOLEAN(true);
  return true;
}

u64 d_u64_read(d_u64 *self, u64 key, u64 hash) {
  if (self->size == 0)
    return GAB_VAL_NULL();

  u64 index = find_index(&self->keys, &self->values, self->capacity, key, hash);

  return v_u64_val_at(&self->values, index);
}

u64 d_u64_index_of(d_u64 *self, u64 key, u64 hash) {
  return find_index(&self->keys, &self->values, self->capacity, key, hash);
}

#include "../gab/compiler/object.h"
boolean d_u64_has_key(d_u64 *self, u64 key, u64 hash) {
  u64 index = find_index(&self->keys, &self->values, self->capacity, key, hash);

  return gab_val_equal(key, v_u64_val_at(&self->keys, index));
}
