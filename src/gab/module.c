#include "include/module.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/object.h"
#include <stdio.h>

gab_module *gab_module_create(gab_module *self, s_i8 name, s_i8 source) {
  self->name = name;
  self->source = source;

  v_u8_create(&self->bytecode, 256);
  v_u8_create(&self->tokens, 256);
  v_u64_create(&self->lines, 256);

  d_gab_constant_create(&self->constants, MODULE_CONSTANTS_MAX);

  self->next = NULL;
  return self;
}

void gab_module_destroy(gab_module *self) {
  v_u8_destroy(&self->bytecode);
  v_u8_destroy(&self->tokens);
  v_u64_destroy(&self->lines);
  d_gab_constant_destroy(&self->constants);
  v_s_i8_destroy(self->source_lines);

  DESTROY(self->source_lines);
  DESTROY(self);
}

void gab_module_dref_all(gab_engine *gab, gab_module *mod) {
  for (u64 i = 0; i < mod->constants.cap; i++) {
    if (d_gab_constant_iexists(&mod->constants, i)) {
      gab_value v = d_gab_constant_ikey(&mod->constants, i);
      gab_dref(gab, v);
    }
  }
}

// Helper macros for creating the specialized instructions
#define MAKE_RETURN(n) (OP_RETURN_1 + (n - 1))
#define MAKE_CALL(n) (OP_CALL_0 + (n))
#define MAKE_STORE_LOCAL(n) (OP_STORE_LOCAL_0 + (n))
#define MAKE_LOAD_LOCAL(n) (OP_LOAD_LOCAL_0 + (n))
#define MAKE_STORE_UPVALUE(n) (OP_STORE_UPVALUE_0 + (n))
#define MAKE_LOAD_UPVALUE(n) (OP_LOAD_UPVALUE_0 + (n))
#define MAKE_LOAD_CONST_UPVALUE(n) (OP_LOAD_CONST_UPVALUE_0 + (n))

/*
  Helpers for pushing ops into the module.
*/
void gab_module_push_op(gab_module *self, gab_opcode op, gab_token t, u64 l) {
  self->previous_compiled_op = op;
  v_u8_push(&self->bytecode, op);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
};

u8 replace_previous_op(gab_module *self, gab_opcode op) {
  v_u8_set(&self->bytecode, self->bytecode.len - 1, op);
  self->previous_compiled_op = op;
  return op;
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
                                u64 l, boolean mutable) {
  if (upvalue < 9) {
    u8 op =
        mutable ? MAKE_LOAD_UPVALUE(upvalue) : MAKE_LOAD_CONST_UPVALUE(upvalue);
    gab_module_push_op(self, op, t, l);
    return op;
  }

  gab_module_push_op(self, mutable ? OP_LOAD_UPVALUE : OP_LOAD_CONST_UPVALUE, t,
                     l);
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

u8 gab_module_push_pop(gab_module *self, u8 n, gab_token t, u64 l) {
  if (n == 1) {
    u8 op = OP_POP;
    // Perform a simple optimazation
    switch (self->previous_compiled_op) {
    case OP_STORE_LOCAL_0:
    case OP_STORE_LOCAL_1:
    case OP_STORE_LOCAL_2:
    case OP_STORE_LOCAL_3:
    case OP_STORE_LOCAL_4:
    case OP_STORE_LOCAL_5:
    case OP_STORE_LOCAL_6:
    case OP_STORE_LOCAL_7:
    case OP_STORE_LOCAL_8:
      return replace_previous_op(self, self->previous_compiled_op + 9);
    }

    gab_module_push_op(self, op, t, l);
    return OP_POP;
  }

  gab_module_push_op(self, OP_POP_N, t, l);
  gab_module_push_byte(self, n, t, l);
  return OP_POP_N;
}

void gab_module_push_inline_cache(gab_module *self, gab_token t, u64 l) {
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
}

u64 gab_module_push_jump(gab_module *self, u8 op, gab_token t, u64 l) {
  gab_module_push_byte(self, op, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  gab_module_push_byte(self, OP_NOP, t, l);
  return self->bytecode.len - 2;
}

void gab_module_patch_jump(gab_module *self, u64 jump) {
  u64 dist = self->bytecode.len - jump - 2;

  assert(dist < UINT16_MAX);

  v_u8_set(&self->bytecode, jump, (dist >> 8) & 0xff);
  v_u8_set(&self->bytecode, jump + 1, dist & 0xff);
}

void gab_module_push_loop(gab_module *self, u64 dist, gab_token t, u64 l) {
  assert(dist < UINT16_MAX);

  u16 jump = self->bytecode.len - dist + 2;

  gab_module_push_op(self, OP_LOOP, t, l);
  gab_module_push_short(self, jump, t, l);
}

boolean gab_module_try_patch_vse(gab_module *self, u8 want) {
  switch (self->previous_compiled_op) {
  case OP_SPREAD:
  case OP_CALL_0:
  case OP_CALL_1:
  case OP_CALL_2:
  case OP_CALL_3:
  case OP_CALL_4:
  case OP_CALL_5:
  case OP_CALL_6:
  case OP_CALL_7:
  case OP_CALL_8:
  case OP_CALL_9:
  case OP_CALL_10:
  case OP_CALL_11:
  case OP_CALL_12:
  case OP_CALL_13:
  case OP_CALL_14:
  case OP_CALL_15:
  case OP_CALL_16:
    v_u8_set(&self->bytecode, self->bytecode.len - 1, want);
    return true;
  }
  return false;
}

u16 gab_module_add_constant(gab_module *self, gab_value value) {
  if (self->constants.len == MODULE_CONSTANTS_MAX) {
    fprintf(stderr, "Uh oh, too many constants in the module.\n");
    exit(1);
  }
  d_gab_constant_insert(&self->constants, value, 0);
  return d_gab_constant_index_of(&self->constants, value);
};

u64 dumpInstruction(gab_module *self, u64 offset);

u64 dumpSimpleInstruction(gab_module *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s\n", name);
  return offset + 1;
}

u64 dumpByteInstruction(gab_module *self, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s %hhx\n", name, operand);
  return offset + 2;
}

u64 dumpTwoByteInstruction(gab_module *self, u64 offset) {
  u8 operandA = v_u8_val_at(&self->bytecode, offset + 1);
  u8 operandB = v_u8_val_at(&self->bytecode, offset + 2);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s %hhx %hhx\n", name, operandA, operandB);
  return offset + 3;
}

u64 dumpDictInstruction(gab_module *self, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s %hhx\n", name, operand);
  return offset + 2;
};

u64 dumpConstantInstruction(gab_module *self, u64 offset) {
  u16 constant = v_u8_val_at(&self->bytecode, offset + 1) << 8 |
                 v_u8_val_at(&self->bytecode, offset + 2);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s ", name);
  gab_val_dump(d_gab_constant_ikey(&self->constants, constant));
  printf("\n");
  return offset + 3;
}

u64 dumpJumpInstruction(gab_module *self, u64 sign, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  u16 dist = (u16)v_u8_val_at(&self->bytecode, offset + 1) << 8;
  dist |= v_u8_val_at(&self->bytecode, offset + 2);

  printf("%-16s %04lu -> %04lu\n", name, offset, offset + 3 + (sign * (dist)));
  return offset + 3;
}

u64 dumpInstruction(gab_module *self, u64 offset) {
  u8 op = v_u8_val_at(&self->bytecode, offset);
  switch (op) {
  case OP_RETURN_1:
  case OP_RETURN_2:
  case OP_RETURN_3:
  case OP_RETURN_4:
  case OP_RETURN_5:
  case OP_RETURN_6:
  case OP_RETURN_7:
  case OP_RETURN_8:
  case OP_RETURN_9:
  case OP_RETURN_10:
  case OP_RETURN_11:
  case OP_RETURN_12:
  case OP_RETURN_13:
  case OP_RETURN_14:
  case OP_RETURN_15:
  case OP_RETURN_16:
  case OP_STORE_LOCAL_0:
  case OP_STORE_LOCAL_1:
  case OP_STORE_LOCAL_2:
  case OP_STORE_LOCAL_3:
  case OP_STORE_LOCAL_4:
  case OP_STORE_LOCAL_5:
  case OP_STORE_LOCAL_6:
  case OP_STORE_LOCAL_7:
  case OP_STORE_LOCAL_8:
  case OP_POP_STORE_LOCAL_0:
  case OP_POP_STORE_LOCAL_1:
  case OP_POP_STORE_LOCAL_2:
  case OP_POP_STORE_LOCAL_3:
  case OP_POP_STORE_LOCAL_4:
  case OP_POP_STORE_LOCAL_5:
  case OP_POP_STORE_LOCAL_6:
  case OP_POP_STORE_LOCAL_7:
  case OP_POP_STORE_LOCAL_8:
  case OP_LOAD_LOCAL_0:
  case OP_LOAD_LOCAL_1:
  case OP_LOAD_LOCAL_2:
  case OP_LOAD_LOCAL_3:
  case OP_LOAD_LOCAL_4:
  case OP_LOAD_LOCAL_5:
  case OP_LOAD_LOCAL_6:
  case OP_LOAD_LOCAL_7:
  case OP_LOAD_LOCAL_8:
  case OP_LOAD_UPVALUE_0:
  case OP_LOAD_UPVALUE_1:
  case OP_LOAD_UPVALUE_2:
  case OP_LOAD_UPVALUE_3:
  case OP_LOAD_UPVALUE_4:
  case OP_LOAD_UPVALUE_5:
  case OP_LOAD_UPVALUE_6:
  case OP_LOAD_UPVALUE_7:
  case OP_LOAD_UPVALUE_8:
  case OP_LOAD_CONST_UPVALUE_0:
  case OP_LOAD_CONST_UPVALUE_1:
  case OP_LOAD_CONST_UPVALUE_2:
  case OP_LOAD_CONST_UPVALUE_3:
  case OP_LOAD_CONST_UPVALUE_4:
  case OP_LOAD_CONST_UPVALUE_5:
  case OP_LOAD_CONST_UPVALUE_6:
  case OP_LOAD_CONST_UPVALUE_7:
  case OP_LOAD_CONST_UPVALUE_8:
  case OP_STORE_UPVALUE_0:
  case OP_STORE_UPVALUE_1:
  case OP_STORE_UPVALUE_2:
  case OP_STORE_UPVALUE_3:
  case OP_STORE_UPVALUE_4:
  case OP_STORE_UPVALUE_5:
  case OP_STORE_UPVALUE_6:
  case OP_STORE_UPVALUE_7:
  case OP_STORE_UPVALUE_8:
  case OP_PUSH_FALSE:
  case OP_PUSH_NULL:
  case OP_PUSH_TRUE:
  case OP_ADD:
  case OP_ASSERT:
  case OP_TYPE:
  case OP_SUBTRACT:
  case OP_DIVIDE:
  case OP_MULTIPLY:
  case OP_MODULO:
  case OP_NOT:
  case OP_NEGATE:
  case OP_LESSER:
  case OP_LESSER_EQUAL:
  case OP_GREATER_EQUAL:
  case OP_GREATER:
  case OP_SWAP:
  case OP_MATCH:
  case OP_POP:
  case OP_GET_INDEX:
  case OP_SET_INDEX:
  case OP_CONCAT:
  case OP_STRINGIFY:
  case OP_NOP:
  case OP_EQUAL: {
    return dumpSimpleInstruction(self, offset);
  }
  case OP_BITWISE_AND:
  case OP_BITWISE_OR:
  case OP_LOGICAL_AND:
  case OP_JUMP_IF_FALSE:
  case OP_JUMP_IF_TRUE:
  case OP_JUMP:
  case OP_POP_JUMP_IF_FALSE:
  case OP_POP_JUMP_IF_TRUE:
  case OP_LOGICAL_OR: {
    return dumpJumpInstruction(self, 1, offset);
  }
  case OP_LOOP: {
    return dumpJumpInstruction(self, -1, offset);
  }
  case OP_VARCALL:
  case OP_VARRETURN: {
    return dumpTwoByteInstruction(self, offset);
  }
  case OP_CONSTANT: {
    return dumpConstantInstruction(self, offset);
  }
  case OP_GET_PROPERTY: {
    return dumpConstantInstruction(self, offset) + 10;
  }
  case OP_SET_PROPERTY: {
    return dumpConstantInstruction(self, offset) + 10;
  }
  case OP_CALL_0:
  case OP_CALL_1:
  case OP_CALL_2:
  case OP_CALL_3:
  case OP_CALL_4:
  case OP_CALL_5:
  case OP_CALL_6:
  case OP_CALL_7:
  case OP_CALL_8:
  case OP_CALL_9:
  case OP_CALL_10:
  case OP_CALL_11:
  case OP_CALL_12:
  case OP_CALL_13:
  case OP_CALL_14:
  case OP_CALL_15:
  case OP_CALL_16:
  case OP_SPREAD:
  case OP_POP_N:
  case OP_CLOSE_UPVALUE:
  case OP_STORE_LOCAL:
  case OP_STORE_UPVALUE:
  case OP_POP_STORE_LOCAL:
  case OP_POP_STORE_UPVALUE:
  case OP_LOAD_UPVALUE:
  case OP_LOAD_CONST_UPVALUE:
  case OP_LOAD_LOCAL: {
    return dumpByteInstruction(self, offset);
  }
  case OP_CLOSURE: {
    u16 constant = ((((u16)self->bytecode.data[offset + 1]) << 8) |
                    self->bytecode.data[offset + 2]);
    printf("%-16s ", "OP_CLOSURE");
    gab_val_dump(d_gab_constant_ikey(&self->constants, constant));
    printf("\n");
    offset += 3;

    gab_obj_function *function =
        GAB_VAL_TO_FUNCTION(d_gab_constant_ikey(&self->constants, constant));

    for (int j = 0; j < function->nupvalues; j++) {
      int flags = self->bytecode.data[offset++];
      int index = self->bytecode.data[offset++];
      int isLocal = flags & FLAG_LOCAL;
      int isMutable = flags & FLAG_MUTABLE;
      printf("%04lu      |                     %d %s %s\n", offset - 2, index,
             isLocal ? "local" : "upvalue", isMutable ? "mut" : "const");
    }
    return offset;
  }
  case OP_OBJECT_RECORD_DEF: {
    offset += 2;
  }
  case OP_OBJECT_RECORD: {
    return dumpDictInstruction(self, offset);
  }
  default: {
    u8 code = v_u8_val_at(&self->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

void gab_module_dump(gab_module *self, s_i8 name) {
  if (self) {
    printf("     %.*s\n", (int)name.len, name.data);
    u64 offset = 0;
    while (offset < self->bytecode.len) {
      printf("%04lu ", offset);
      offset = dumpInstruction(self, offset);
    }
  }
}
