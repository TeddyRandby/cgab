#include "module.h"
#include <stdio.h>

gab_module *gab_module_create(gab_module *self, s_u8_ref name, s_u8 *source) {
  self->name = name;
  self->source = source;
  v_u8_create(&self->bytecode);
  v_u8_create(&self->tokens);
  v_u64_create(&self->lines);

  self->next = NULL;
  return self;
}

void gab_module_destroy(gab_module *self) {
  s_u8_destroy(self->source);

  v_u8_destroy(&self->bytecode);
  v_u8_destroy(&self->tokens);
  v_u64_destroy(&self->lines);
  v_s_u8_ref_destroy(self->source_lines);

  DESTROY_STRUCT(self->source_lines);
  DESTROY_STRUCT(self);
}

void gab_module_push_byte(gab_module *self, gab_opcode op, gab_token token,
                          u64 line) {
  v_u8_push(&self->bytecode, op);
  v_u8_push(&self->tokens, token);
  v_u64_push(&self->lines, line);
}
