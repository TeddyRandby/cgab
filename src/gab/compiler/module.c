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

// Helper macros for creating the specialized instructions
#define MAKE_RETURN(n) (OP_RETURN_1 + (n - 1))
#define MAKE_CALL(n) (OP_CALL_0 + (n))
#define MAKE_STORE_LOCAL(n) (OP_STORE_LOCAL_0 + (n))
#define MAKE_LOAD_LOCAL(n) (OP_LOAD_LOCAL_0 + (n))
#define MAKE_STORE_UPVALUE(n) (OP_STORE_UPVALUE_0 + (n))
#define MAKE_LOAD_UPVALUE(n) (OP_LOAD_UPVALUE_0 + (n))

/*
  Helpers for pushing ops into the module.
*/
void gab_module_push_op(gab_module *self, gab_opcode op, gab_token t, u64 l) {
  self->previous_compiled_op = op;
  v_u8_push(&self->bytecode, op);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
};
void gab_module_push_byte(gab_module *self, u8 data, gab_token t, u64 l) {
  v_u8_push(&self->bytecode, data);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
}

void gab_module_push_short(gab_module *self, u16 data, gab_token t, u64 l) {
  gab_module_push_byte(self, (data >> 8) & 0xff, t, l);
  gab_module_push_byte(self, data & 0xff, t, l);
};

/* These helpers return the instruction they push. */
u8 gab_module_push_load_local(gab_module *self, u8 local, gab_token t, u64 l) {
  if (local < 9) {
    u8 op = MAKE_LOAD_LOCAL(local);
    gab_module_push_op(self, op, t, l);
    return op;
  }

  gab_module_push_op(self, OP_LOAD_LOCAL, t, l);
  gab_module_push_byte(self, local, t, l);

  return OP_LOAD_LOCAL;
}

u8 gab_module_push_load_upvalue(gab_module *self, u8 upvalue, gab_token t,
                                u64 l) {
  if (upvalue < 9) {
    u8 op = MAKE_LOAD_UPVALUE(upvalue);
    gab_module_push_op(self, op, t, l);
    return op;
  }

  gab_module_push_op(self, OP_LOAD_UPVALUE, t, l);
  gab_module_push_byte(self, upvalue, t, l);

  return OP_LOAD_UPVALUE;
};

u8 gab_module_push_store_local(gab_module *self, u8 local, gab_token t, u64 l) {
  if (local < 9) {
    u8 op = MAKE_STORE_LOCAL(local);
    gab_module_push_op(self, op, t, l);
    return op;
  }

  gab_module_push_op(self, OP_STORE_LOCAL, t, l);
  gab_module_push_byte(self, local, t, l);

  return OP_STORE_LOCAL;
};

u8 gab_module_push_store_upvalue(gab_module *self, u8 upvalue, gab_token t,
                                 u64 l) {
  if (upvalue < 9) {
    u8 op = MAKE_STORE_UPVALUE(upvalue);
    gab_module_push_op(self, op, t, l);
    return op;
  }

  gab_module_push_op(self, OP_STORE_UPVALUE, t, l);
  gab_module_push_byte(self, upvalue, t, l);

  return OP_STORE_UPVALUE;
};

u8 gab_module_push_return(gab_module *self, u8 have, u8 var, gab_token t,
                          u64 l) {
  if (!var) {
    u8 op = MAKE_RETURN(have);
    gab_module_push_op(self, op, t, l);
    return op;
  }
  gab_module_push_op(self, OP_VARRETURN, t, l);
  gab_module_push_byte(self, have, t, l);
  return OP_VARRETURN;
}

u8 gab_module_push_call(gab_module *self, u8 have, u8 var, gab_token t, u64 l) {
  if (!var) {
    u8 op = MAKE_CALL(have);
    gab_module_push_op(self, op, t, l);
    gab_module_push_byte(self, 1, t, l);
    return op;
  }
  gab_module_push_op(self, OP_VARCALL, t, l);
  gab_module_push_byte(self, have, t, l);
  gab_module_push_byte(self, 1, t, l);
  return OP_VARCALL;
}

void gab_module_push_inline_cache(gab_module *self, gab_token t, u64 l) {
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
}

u64 gab_module_push_jump(gab_module *self, u8 op, gab_token t, u64 l) {
  gab_module_push_byte(self, op, t, l);
  gab_module_push_byte(self, 0, t, l);
  gab_module_push_byte(self, 0, t, l);
  return self->bytecode.size - 2;
}

void gab_module_patch_jump(gab_module *self, u64 jump) {
  u64 dist = self->bytecode.size - jump - 2;

  ASSERT_TRUE(dist < UINT16_MAX, "Cannot generate this large a jump");

  v_u8_set(&self->bytecode, jump, (dist >> 8) & 0xff);
  v_u8_set(&self->bytecode, jump + 1, dist & 0xff);
}

void gab_module_push_loop(gab_module *self, u64 dist, gab_token t, u64 l) {
  ASSERT_TRUE(dist < UINT16_MAX, "Cannot generate this large a jump");

  u16 jump = self->bytecode.size - dist + 2;

  gab_module_push_op(self, OP_LOOP, t, l);
  gab_module_push_short(self, jump, t, l);
}

void gab_module_try_patch_vse(gab_module *self, u8 want) {
  if (self->previous_compiled_op == OP_SPREAD ||
      (self->previous_compiled_op >= OP_CALL_0 &&
       self->previous_compiled_op <= OP_CALL_16)) {
    v_u8_set(&self->bytecode, self->bytecode.size - 1, want);
  }
}
