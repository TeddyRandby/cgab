#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

s_u8_ref s_u8_ref_create(u8 *data, u64 size) {
  s_u8_ref self = {.size = size, .data = data};
  return self;
}

s_u8_ref s_u8_ref_create_cstr(const char *data) {
  s_u8_ref self = {.size = strlen(data), .data = (u8 *)data};
  return self;
}

s_u8_ref s_u8_ref_create_s_u8(s_u8 *data) {
  s_u8_ref self = {.size = data->size, .data = data->data};
  return self;
}

s_u8_ref s_u8_ref_copy(s_u8_ref other);

boolean s_u8_ref_match(s_u8_ref a, s_u8_ref b) {
  if (a.size != b.size)
    return false;
  return memcmp(a.data, b.data, a.size) == 0;
}

boolean s_u8_ref_match_lit(s_u8_ref a, const char *b) {
  s_u8_ref other = s_u8_ref_create((u8 *)b, strlen(b));
  return s_u8_ref_match(a, other);
}

s_u8 *s_u8_create_cstr(const char *cstr) {
  u64 length = strlen(cstr);

  s_u8 *self = CREATE_FLEX_STRUCT(s_u8, u8, length + 1);
  self->size = length;

  COPY(self->data, cstr, length + 1);

  return self;
}

s_u8 *s_u8_create_sref(const s_u8_ref other) {
  s_u8 *self = CREATE_FLEX_STRUCT(s_u8, u8, other.size + 1);
  self->size = other.size;
  COPY(self->data, other.data, other.size);
  self->data[self->size] = '\0';
  return self;
}

s_u8 *s_u8_create_empty(u64 size) {
  s_u8 *self = CREATE_FLEX_STRUCT(s_u8, u8, size + 1);
  memset(self->data, 0, size);
  self->size = size;
  self->data[size] = '\0';
  return self;
}

// Assumes the s_u8 is the right size
void s_u8_copy_ref(s_u8 *self, s_u8_ref other) {
  COPY(self->data, other.data, other.size);
  self->size = other.size;
  self->data[other.size] = '\0';
}

s_u8 *s_u8_append_s_u8(s_u8 *self, s_u8 *other, boolean free_self) {
  if (self == NULL)
    return other;
  s_u8 *new_str = CREATE_FLEX_STRUCT(s_u8, u8, self->size + other->size);
  new_str->size = self->size + other->size;
  COPY(new_str->data, self->data, self->size);
  COPY(new_str->data + self->size, other->data, other->size);
  if (free_self) {
    s_u8_destroy(self);
  }
  return new_str;
}

boolean s_u8_match_ref(s_u8 *self, s_u8_ref other) {
  if (self->size != other.size)
    return false;
  return memcmp(self->data, other.data, other.size) == 0;
}

boolean s_u8_match_cstr(s_u8 *self, const char *other) {
  if (self->size != strlen(other))
    return false;
  return memcmp(self->data, other, self->size) == 0;
}

void s_u8_destroy(s_u8 *self) { DESTROY_STRUCT(self); }
