#include "assert.h"
#include "common.h"
#include "memory.h"
#include <stdlib.h>
// ~ String ref vector begin
void v_s_u8_ref_create(v_s_u8_ref *self) {
  self->capacity = 8;
  self->size = 0;
  self->data = CREATE_ARRAY(s_u8_ref, self->capacity);
}

void v_s_u8_ref_destroy(v_s_u8_ref *self) { DESTROY_ARRAY(self->data); }

// ~ 8 bit vector begin
void v_u8_create(v_u8 *self) {
  self->capacity = 8;
  self->size = 0;

  self->data = CREATE_ARRAY(u8, self->capacity);
}

void v_u8_destroy(v_u8 *self) { DESTROY_ARRAY(self->data); }

// ~ 64 bit vector begin
void v_u64_create(v_u64 *self) {
  self->capacity = 8;
  self->size = 0;
  self->data = CREATE_ARRAY(u64, self->capacity);
}

void v_u64_destroy(v_u64 *self) {
  DESTROY_ARRAY(self->data);
  self->capacity = 0;
  self->size = 0;
  self->data = NULL;
}
