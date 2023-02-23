#include "include/module.h"
#include "include/colors.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/lexer.h"
#include "include/object.h"
#include <stdio.h>

gab_module *gab_module_create(gab_value name, gab_source *source,
                              gab_module *next) {
  gab_module *self = NEW(gab_module);
  self->source = source;
  self->main = 0;
  self->previous_compiled_op = OP_NOP;
  self->next = next;

  v_u8_create(&self->bytecode, 256);
  v_u8_create(&self->tokens, 256);
  v_u64_create(&self->lines, 256);
  v_s_i8_create(&self->sources, 256);

  v_gab_constant_create(&self->constants, CONSTANTS_INITIAL_CAP);
  self->name = gab_module_add_constant(self, name);
  return self;
}
void gab_module_collect(gab_engine *gab, gab_module *mod) {
  if (!mod)
    return;

  for (u64 i = 0; i < mod->constants.len; i++) {
    gab_value v = v_gab_constant_val_at(&mod->constants, i);
    // The only kind of value owned by the modules
    // are their prototypes and the main closure
    if (GAB_VAL_IS_SYMBOL(v) || GAB_VAL_IS_PROTOTYPE(v) ||
        GAB_VAL_IS_BLOCK(v)) {
      gab_dref(gab, NULL, v);
    }
  }
}

void gab_module_destroy(gab_engine *gab, gab_module *mod) {
  if (!mod)
    return;

  v_u8_destroy(&mod->bytecode);
  v_u8_destroy(&mod->tokens);
  v_u64_destroy(&mod->lines);
  v_gab_constant_destroy(&mod->constants);
  v_s_i8_destroy(&mod->sources);

  DESTROY(mod);
}

// Helper macros for creating the specialized instructions
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

u8 replace_previous_op(gab_module *self, gab_opcode op, u8 args) {
  v_u8_set(&self->bytecode, self->bytecode.len - 1 - args, op);
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
        return replace_previous_op(self, self->previous_compiled_op - 9, 0);
    }
    }
    gab_module_push_op(self, op, t, l, s);
    return op;
  }

  gab_module_push_op(self, OP_LOAD_LOCAL, t, l, s);
  gab_module_push_byte(self, local, t, l, s);

  return OP_LOAD_LOCAL;
}

u8 gab_module_push_load_upvalue(gab_module *self, u8 upvalue, boolean mutable,
                                gab_token t, u64 l, s_i8 s) {
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

u8 gab_module_push_return(gab_module *self, u8 have, boolean vse, gab_token t,
                          u64 l, s_i8 s) {
  assert(have < 16);

  gab_module_push_op(self, OP_RETURN, t, l, s);
  gab_module_push_byte(self, (have << 1) | vse, t, l, s);

  return OP_RETURN;
}

u8 gab_module_push_tuple(gab_module *self, u8 have, boolean vse, gab_token t,
                         u64 l, s_i8 s) {
  assert(have < 128);

  gab_module_push_op(self, OP_TUPLE, t, l, s);
  gab_module_push_byte(self, (have << 1) | vse, t, l, s);

  return OP_RETURN;
}

u8 gab_module_push_yield(gab_module *self, u8 have, boolean vse, gab_token t,
                         u64 l, s_i8 s) {
  assert(have < 16);

  gab_module_push_op(self, OP_YIELD, t, l, s);
  gab_module_push_byte(self, (have << 1) | vse, t, l, s);
  gab_module_push_byte(self, 1, t, l, s);
  return OP_YIELD;
}

u8 gab_module_push_send(gab_module *self, u8 have, u16 message, boolean vse,
                        gab_token t, u64 l, s_i8 s) {
  assert(have < 16);

  gab_module_push_op(self, OP_SEND_ANA, t, l, s);
  gab_module_push_short(self, message, t, l, s);
  gab_module_push_byte(self, (have << 1) | vse, t, l, s);
  gab_module_push_byte(self, 1, t, l, s);

  gab_module_push_byte(self, OP_NOP, t, l, s); // Version
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_inline_cache(self, t, l, s);

  return OP_SEND_ANA;
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
      return replace_previous_op(self, self->previous_compiled_op + 9, 0);
    case OP_STORE_LOCAL:
      return replace_previous_op(self, OP_POP_STORE_LOCAL, 1);
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

void gab_module_push_next(gab_module *self, u8 iter, gab_token t, u64 l,
                          s_i8 s) {
  gab_module_push_byte(self, OP_NEXT, t, l, s);
  gab_module_push_byte(self, iter, t, l, s);
}

u64 gab_module_push_iter(gab_module *self, u8 nlocals, u8 start, gab_token t,
                         u64 l, s_i8 s) {
  gab_module_push_byte(self, OP_ITER, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, OP_NOP, t, l, s);
  gab_module_push_byte(self, nlocals, t, l, s);
  gab_module_push_byte(self, start, t, l, s);

  return self->bytecode.len - 4;
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
    v_u8_set(&self->bytecode, self->bytecode.len - 18, want);
    return true;
  case OP_YIELD:
    v_u8_set(&self->bytecode, self->bytecode.len - 1, want);
    return true;
  }
  return false;
}

u16 gab_module_add_constant(gab_module *self, gab_value value) {
  if (self->constants.len >= CONSTANTS_MAX) {
    fprintf(stderr, "Uh oh, too many constants in the module.\n");
    exit(1);
  }
  v_gab_constant_push(&self->constants, value);
  return self->constants.len - 1;
};

u64 dumpInstruction(gab_module *self, u64 offset);

u64 dumpSimpleInstruction(gab_module *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-25s\n", name);
  return offset + 1;
}

u64 dumpSendInstruction(gab_module *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  u16 constant = v_u8_val_at(&self->bytecode, offset + 1) << 8 |
                 v_u8_val_at(&self->bytecode, offset + 2);

  gab_value msg = v_gab_constant_val_at(&self->constants, constant);

  u8 have = v_u8_val_at(&self->bytecode, offset + 3);
  u8 want = v_u8_val_at(&self->bytecode, offset + 4);

  printf("%-25s" ANSI_COLOR_BLUE "%-17V" ANSI_COLOR_RESET
         "%03d args -> %03d results\n",
         name, msg, have, want);
  return offset + 22;
}

u64 dumpByteInstruction(gab_module *self, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-25s%hhx\n", name, operand);
  return offset + 2;
}

u64 dumpTwoByteInstruction(gab_module *self, u64 offset) {
  u8 operandA = v_u8_val_at(&self->bytecode, offset + 1);
  u8 operandB = v_u8_val_at(&self->bytecode, offset + 2);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-25s%hhx %hhx\n", name, operandA, operandB);
  return offset + 3;
}

u64 dumpDictInstruction(gab_module *self, u8 i, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[i];
  printf("%-25s%hhx\n", name, operand);
  return offset + 2;
};

u64 dumpConstantInstruction(gab_module *self, u64 offset) {
  u16 constant = v_u8_val_at(&self->bytecode, offset + 1) << 8 |
                 v_u8_val_at(&self->bytecode, offset + 2);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  printf("%-25s", name);
  gab_val_dump(stdout, v_gab_constant_val_at(&self->constants, constant));
  printf("\n");
  return offset + 3;
}

u64 dumpJumpInstruction(gab_module *self, u64 sign, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  u16 dist = (u16)v_u8_val_at(&self->bytecode, offset + 1) << 8;
  dist |= v_u8_val_at(&self->bytecode, offset + 2);

  printf("%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
         " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET "\n",
         name, offset, offset + 3 + (sign * (dist)));
  return offset + 3;
}

u64 dumpIter(gab_module *self, u64 offset) {
  u16 dist = (u16)v_u8_val_at(&self->bytecode, offset + 1) << 8;
  dist |= v_u8_val_at(&self->bytecode, offset + 2);

  u8 nlocals = v_u8_val_at(&self->bytecode, offset + 3);
  u8 start = v_u8_val_at(&self->bytecode, offset + 4);

  printf("%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
         " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
         " %03d locals from %03d\n",
         "ITER", offset, offset + 3 + dist, nlocals, start);

  return offset + 5;
}

u64 dumpNext(gab_module *self, u64 offset) {
  printf("%-25s\n", "NEXT");
  return offset + 1;
}

u64 dumpInstruction(gab_module *self, u64 offset) {
  u8 op = v_u8_val_at(&self->bytecode, offset);
  switch (op) {
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
  case OP_PUSH_UNDEFINED:
  case OP_SWAP:
  case OP_DUP:
  case OP_NOT:
  case OP_MATCH:
  case OP_POP:
  case OP_INTERPOLATE:
  case OP_TYPE:
  case OP_NOP: {
    return dumpSimpleInstruction(self, offset);
  }
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
  case OP_CONSTANT:
    return dumpConstantInstruction(self, offset);
  case OP_NEXT:
    return dumpNext(self, offset);
  case OP_ITER:
    return dumpIter(self, offset);
  case OP_STORE_PROPERTY_ANA:
  case OP_STORE_PROPERTY_MONO:
  case OP_STORE_PROPERTY_POLY:
  case OP_LOAD_PROPERTY_ANA:
  case OP_LOAD_PROPERTY_MONO:
  case OP_LOAD_PROPERTY_POLY:
    return dumpConstantInstruction(self, offset) + 10;
  case OP_SEND_ANA:
  case OP_SEND_MONO_CLOSURE:
  case OP_SEND_MONO_BUILTIN:
  case OP_SEND_PRIMITIVE_STORE_ANA:
  case OP_SEND_PRIMITIVE_STORE_MONO:
  case OP_SEND_PRIMITIVE_LOAD_ANA:
  case OP_SEND_PRIMITIVE_LOAD_MONO:
  case OP_SEND_PRIMITIVE_CONCAT:
  case OP_SEND_PRIMITIVE_ADD:
  case OP_SEND_PRIMITIVE_SUB:
  case OP_SEND_PRIMITIVE_MUL:
  case OP_SEND_PRIMITIVE_DIV:
  case OP_SEND_PRIMITIVE_MOD:
  case OP_SEND_PRIMITIVE_EQ:
  case OP_SEND_PRIMITIVE_LT:
  case OP_SEND_PRIMITIVE_LTE:
  case OP_SEND_PRIMITIVE_GT:
  case OP_SEND_PRIMITIVE_GTE:
    return dumpSendInstruction(self, offset);
  case OP_RETURN:
  case OP_YIELD:
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

    gab_value pval = v_gab_constant_val_at(&self->constants, proto_constant);
    gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    s_i8 func_name = gab_obj_string_ref(GAB_VAL_TO_STRING(
        v_gab_constant_val_at(&p->mod->constants, p->mod->name)));

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_MESSAGE",
           (int)func_name.len, func_name.data);

    for (int j = 0; j < p->nupvalues; j++) {
      u8 flags = p->upv_desc[j * 2];
      u8 index = p->upv_desc[j * 2 + 1];
      int isLocal = flags & GAB_VARIABLE_FLAG_LOCAL;
      int isMutable = flags & GAB_VARIABLE_FLAG_MUTABLE;
      printf("      |                   %d %s %s\n", index,
             isLocal ? "local" : "upvalue", isMutable ? "mut" : "const");
    }
    return offset;
  }
  case OP_BLOCK: {
    offset++;
    u16 proto_constant = ((((u16)self->bytecode.data[offset]) << 8) |
                          self->bytecode.data[offset + 1]);
    offset += 2;

    gab_value pval = v_gab_constant_val_at(&self->constants, proto_constant);
    gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    s_i8 func_name = gab_obj_string_ref(GAB_VAL_TO_STRING(
        v_gab_constant_val_at(&p->mod->constants, p->mod->name)));

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_BLOCK",
           (int)func_name.len, func_name.data);

    for (int j = 0; j < p->nupvalues; j++) {
      u8 flags = p->upv_desc[j * 2];
      u8 index = p->upv_desc[j * 2 + 1];
      int isLocal = flags & GAB_VARIABLE_FLAG_LOCAL;
      int isMutable = flags & GAB_VARIABLE_FLAG_MUTABLE;
      printf("      |                   %d %s %s\n", index,
             isLocal ? "local" : "upvalue", isMutable ? "mut" : "const");
    }
    return offset;
  }
  case OP_RECORD_DEF:
    offset += 2;
  case OP_RECORD: {
    return dumpDictInstruction(self, op, offset);
  }
  case OP_TUPLE: {
    return dumpDictInstruction(self, op, offset);
  }
  default: {
    u8 code = v_u8_val_at(&self->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

void gab_dis(gab_module *mod) {
  u64 offset = 0;
  while (offset < mod->bytecode.len) {
    printf(ANSI_COLOR_YELLOW "%04lu " ANSI_COLOR_RESET, offset);
    offset = dumpInstruction(mod, offset);
  }
}

void gab_module_dump(gab_module *self, s_i8 name) {
  printf("     %.*s\n", (int)name.len, name.data);
  gab_dis(self);
}
