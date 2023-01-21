#include "include/module.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/object.h"
#include <stdio.h>

gab_module *gab_module_create(gab_module *self, s_i8 name, s_i8 source) {
  self->name = name;
  self->source = a_i8_create(source.data, source.len);
  self->previous_compiled_op = OP_NOP;
  self->main = 0;

  v_u8_create(&self->bytecode, 256);
  v_u8_create(&self->tokens, 256);
  v_u64_create(&self->lines, 256);
  v_s_i8_create(&self->sources, 256);

  d_gab_constant_create(&self->constants, MODULE_CONSTANTS_MAX);
  return self;
}

void gab_module_cleanup(gab_engine *gab, gab_module *mod) {
  if (mod == NULL)
    return;

  for (u64 i = 0; i < mod->constants.cap; i++) {
    if (d_gab_constant_iexists(&mod->constants, i)) {
      gab_value v = d_gab_constant_ikey(&mod->constants, i);
      // The only kind of value owned by the modules
      // that and it's main closure
      if (GAB_VAL_IS_PROTOTYPE(v) || GAB_VAL_IS_CLOSURE(v))
        gab_dref(gab, NULL, v);
    }
  }
}

void gab_module_destroy(gab_module *mod) {
  if (mod == NULL)
    return;

  v_u8_destroy(&mod->bytecode);
  v_u8_destroy(&mod->tokens);
  v_u64_destroy(&mod->lines);
  d_gab_constant_destroy(&mod->constants);
  v_s_i8_destroy(&mod->sources);
  if (mod->source_lines) {
    v_s_i8_destroy(mod->source_lines);
    DESTROY(mod->source_lines);
  }

  a_i8_destroy(mod->source);

  DESTROY(mod);
}

// Helper macros for creating the specialized instructions
#define MAKE_RETURN(n) (OP_RETURN_1 + (n - 1))
#define MAKE_SEND(n) (OP_SEND_0 + (n))
#define MAKE_STORE_LOCAL(n) (OP_STORE_LOCAL_0 + (n))
#define MAKE_LOAD_LOCAL(n) (OP_LOAD_LOCAL_0 + (n))
#define MAKE_STORE_UPVALUE(n) (OP_STORE_UPVALUE_0 + (n))
#define MAKE_LOAD_UPVALUE(n) (OP_LOAD_UPVALUE_0 + (n))
#define MAKE_LOAD_CONST_UPVALUE(n) (OP_LOAD_CONST_UPVALUE_0 + (n))

/*
  Helpers for pushing ops into the module.
*/
void gab_module_push_op(gab_module *self, gab_opcode op, gab_token t, u64 l,
                        s_i8 s) {
  self->previous_compiled_op = op;
  v_u8_push(&self->bytecode, op);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
  v_s_i8_push(&self->sources, s);
};

u8 replace_previous_op(gab_module *self, gab_opcode op) {
  v_u8_set(&self->bytecode, self->bytecode.len - 1, op);
  self->previous_compiled_op = op;
  return op;
};

void gab_module_push_byte(gab_module *self, u8 data, gab_token t, u64 l,
                          s_i8 s) {
  v_u8_push(&self->bytecode, data);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
  v_s_i8_push(&self->sources, s);
}

void gab_module_push_short(gab_module *self, u16 data, gab_token t, u64 l,
                           s_i8 s) {
  gab_module_push_byte(self, (data >> 8) & 0xff, t, l, s);
  gab_module_push_byte(self, data & 0xff, t, l, s);
  v_s_i8_push(&self->sources, s);
};

/* These helpers return the instruction they push. */
u8 gab_module_push_load_local(gab_module *self, u8 local, gab_token t, u64 l,
                              s_i8 s) {
  if (local < 9) {
    u8 op = MAKE_LOAD_LOCAL(local);
    switch (self->previous_compiled_op) {
    case OP_POP_STORE_LOCAL_0:
    case OP_POP_STORE_LOCAL_1:
    case OP_POP_STORE_LOCAL_2:
    case OP_POP_STORE_LOCAL_3:
    case OP_POP_STORE_LOCAL_4:
    case OP_POP_STORE_LOCAL_5:
    case OP_POP_STORE_LOCAL_6:
    case OP_POP_STORE_LOCAL_7:
    case OP_POP_STORE_LOCAL_8: {
      if (op == self->previous_compiled_op - 9)
        return replace_previous_op(self, self->previous_compiled_op - 9);
    }
    }
    gab_module_push_op(self, op, t, l, s);
    return op;
  }

  gab_module_push_op(self, OP_LOAD_LOCAL, t, l, s);
  gab_module_push_byte(self, local, t, l, s);

  return OP_LOAD_LOCAL;
}

u8 gab_module_push_load_upvalue(gab_module *self, u8 upvalue, gab_token t,
                                u64 l, s_i8 s, boolean mutable) {
  if (upvalue < 9) {
    u8 op =
        mutable ? MAKE_LOAD_UPVALUE(upvalue) : MAKE_LOAD_CONST_UPVALUE(upvalue);
    gab_module_push_op(self, op, t, l, s);
    return op;
  }

  gab_module_push_op(self, mutable ? OP_LOAD_UPVALUE : OP_LOAD_CONST_UPVALUE, t,
                     l, s);
  gab_module_push_byte(self, upvalue, t, l, s);

  return OP_LOAD_UPVALUE;
};

u8 gab_module_push_store_local(gab_module *self, u8 local, gab_token t, u64 l,
                               s_i8 s) {
  if (local < 9) {
    u8 op = MAKE_STORE_LOCAL(local);
    gab_module_push_op(self, op, t, l, s);
    return op;
  }

  gab_module_push_op(self, OP_STORE_LOCAL, t, l, s);
  gab_module_push_byte(self, local, t, l, s);

  return OP_STORE_LOCAL;
};

u8 gab_module_push_store_upvalue(gab_module *self, u8 upvalue, gab_token t,
                                 u64 l, s_i8 s) {
  if (upvalue < 9) {
    u8 op = MAKE_STORE_UPVALUE(upvalue);
    gab_module_push_op(self, op, t, l, s);
    return op;
  }

  gab_module_push_op(self, OP_STORE_UPVALUE, t, l, s);
  gab_module_push_byte(self, upvalue, t, l, s);

  return OP_STORE_UPVALUE;
};

u8 gab_module_push_return(gab_module *self, u8 have, u8 var, gab_token t, u64 l,
                          s_i8 s) {
  if (!var) {
    u8 op = MAKE_RETURN(have);
    gab_module_push_op(self, op, t, l, s);
    return op;
  }
  gab_module_push_op(self, OP_VARRETURN, t, l, s);
  gab_module_push_byte(self, have, t, l, s);
  return OP_VARRETURN;
}
u8 gab_module_push_yield(gab_module *self, u8 have, u8 var, gab_token t, u64 l,
                         s_i8 s) {
  u8 op = var ? OP_VARYIELD : OP_YIELD;

  gab_module_push_op(self, op, t, l, s);
  gab_module_push_byte(self, have, t, l, s);
  gab_module_push_byte(self, 1, t, l, s);
  return OP_YIELD;
}

u8 gab_module_push_send(gab_module *self, u8 have, u8 var, u16 message,
                        gab_token t, u64 l, s_i8 s) {
  u8 op = var ? OP_VARSEND_ANA : OP_SEND_ANA;

  gab_module_push_op(self, op, t, l, s);
  gab_module_push_short(self, message, t, l, s);
  gab_module_push_byte(self, have, t, l, s);
  gab_module_push_byte(self, 1, t, l, s);

  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_inline_cache(self, t, l, s);

  return op;
}

u8 gab_module_push_dynsend(gab_module *self, u8 have, u8 var, gab_token t,
                           u64 l, s_i8 s) {

  u8 op = var ? OP_VARDYNSEND : OP_DYNSEND;

  gab_module_push_op(self, op, t, l, s);
  gab_module_push_byte(self, have, t, l, s);
  gab_module_push_byte(self, 1, t, l, s);

  return op;
}

u8 gab_module_push_pop(gab_module *self, u8 n, gab_token t, u64 l, s_i8 s) {
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

    gab_module_push_op(self, op, t, l, s);
    return OP_POP;
  }

  gab_module_push_op(self, OP_POP_N, t, l, s);
  gab_module_push_byte(self, n, t, l, s);
  return OP_POP_N;
}

void gab_module_push_inline_cache(gab_module *self, gab_token t, u64 l,
                                  s_i8 s) {
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
}

u64 gab_module_push_jump(gab_module *self, u8 op, gab_token t, u64 l, s_i8 s) {
  gab_module_push_byte(self, op, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  return self->bytecode.len - 2;
}

void gab_module_patch_jump(gab_module *self, u64 jump) {
  u64 dist = self->bytecode.len - jump - 2;

  assert(dist < UINT16_MAX);

  v_u8_set(&self->bytecode, jump, (dist >> 8) & 0xff);
  v_u8_set(&self->bytecode, jump + 1, dist & 0xff);
}

u64 gab_module_push_loop(gab_module *self) { return self->bytecode.len; }

void gab_module_patch_loop(gab_module *self, u64 start, gab_token t, u64 l,
                           s_i8 s) {
  u64 diff = self->bytecode.len - start + 3;
  assert(diff < UINT16_MAX);

  gab_module_push_op(self, OP_LOOP, t, l, s);
  gab_module_push_short(self, diff, t, l, s);
}

boolean gab_module_try_patch_vse(gab_module *self, u8 want) {
  switch (self->previous_compiled_op) {
  case OP_SEND_ANA:
  case OP_VARSEND_ANA:
    v_u8_set(&self->bytecode, self->bytecode.len - 12, want);
    return true;
  case OP_VARDYNSEND:
  case OP_DYNSEND:
  case OP_VARYIELD:
  case OP_YIELD:
  case OP_SPREAD:
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

u64 dumpDynSendInstruction(gab_module *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s\n", name);
  return offset + 3;
}

u64 dumpSendInstruction(gab_module *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-16s\n", name);
  return offset + 16;
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

u64 dumpDictInstruction(gab_module *self, u8 i, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[i];
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
  case OP_PUSH_NIL:
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
  case OP_DUP:
  case OP_MATCH:
  case OP_POP:
  case OP_LOAD_INDEX:
  case OP_STORE_INDEX:
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
  case OP_LOGICAL_OR:
    return dumpJumpInstruction(self, 1, offset);
  case OP_LOOP:
    return dumpJumpInstruction(self, -1, offset);
  case OP_VARRETURN:
    return dumpTwoByteInstruction(self, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(self, offset);
  case OP_STORE_PROPERTY_ANA:
  case OP_STORE_PROPERTY_MONO:
  case OP_STORE_PROPERTY_POLY:
  case OP_LOAD_PROPERTY_ANA:
  case OP_LOAD_PROPERTY_MONO:
  case OP_LOAD_PROPERTY_POLY:
    return dumpConstantInstruction(self, offset) + 10;
  case OP_VARSEND_ANA:
  case OP_VARSEND_MONO_BUILTIN:
  case OP_VARSEND_MONO_CLOSURE:
  case OP_SEND_ANA:
  case OP_SEND_MONO_CLOSURE:
  case OP_SEND_MONO_BUILTIN:
    return dumpSendInstruction(self, offset);
  case OP_DYNSEND:
  case OP_VARDYNSEND:
    return dumpDynSendInstruction(self, offset);
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
  case OP_MESSAGE: {
    offset++;
    u16 proto_constant = ((((u16)self->bytecode.data[offset]) << 8) |
                          self->bytecode.data[offset + 1]);
    offset += 4;

    gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(
        d_gab_constant_ikey(&self->constants, proto_constant));

    printf("%-16s %.*s\n", "OP_MESSAGE", (i32)p->name.len, p->name.data);

    for (int j = 0; j < p->nupvalues; j++) {
      u8 flags = self->bytecode.data[offset++];
      u8 index = self->bytecode.data[offset++];
      int isLocal = flags & FLAG_LOCAL;
      int isMutable = flags & FLAG_MUTABLE;
      printf("%04lu      |                     %d %s %s\n", offset, index,
             isLocal ? "local" : "upvalue", isMutable ? "mut" : "const");
    }
    return offset;
  }
  case OP_CLOSURE: {
    offset++;
    u16 proto_constant = ((((u16)self->bytecode.data[offset]) << 8) |
                          self->bytecode.data[offset + 1]);
    offset += 2;

    gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(
        d_gab_constant_ikey(&self->constants, proto_constant));

    printf("%-16s %.*s\n", "OP_CLOSURE", (i32)p->name.len, p->name.data);

    for (int j = 0; j < p->nupvalues; j++) {
      u8 flags = self->bytecode.data[offset++];
      u8 index = self->bytecode.data[offset++];
      int isLocal = flags & FLAG_LOCAL;
      int isMutable = flags & FLAG_MUTABLE;
      printf("%04lu      |                     %d %s %s\n", offset, index,
             isLocal ? "local" : "upvalue", isMutable ? "mut" : "const");
    }
    return offset;
  }
  case OP_OBJECT_RECORD_DEF:
    offset += 2;
  case OP_OBJECT_RECORD: {
    return dumpDictInstruction(self, op, offset);
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
