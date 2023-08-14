#include "include/module.h"
#include "include/colors.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/lexer.h"
#include "include/object.h"
#include "include/value.h"
#include <stdio.h>

gab_mod *gab_mod_create(gab_eg *gab, gab_value name, gab_src *source) {
  gab_mod *self = NEW(gab_mod);
  memset(self, 0, sizeof(gab_mod));

  self->name = gab_mod_add_constant(self, name);
  self->source = source;
  self->next = gab->modules;
  gab->modules = self;

  return self;
}

void gab_mod_destroy(gab_eg *gab, gab_gc *gc, gab_mod *mod) {
  if (!mod)
    return;

  for (u64 i = 0; i < mod->constants.len; i++) {
    gab_value v = v_gab_constant_val_at(&mod->constants, i);
    // The only kind of value owned by the modules
    // are their prototypes and the main closure
    if (gab_valknd(v) == kGAB_BLOCK_PROTO || gab_valknd(v) == kGAB_BLOCK) {
      gab_gcdref(gc, NULL, v);
    }
  }

  v_u8_destroy(&mod->bytecode);
  v_u8_destroy(&mod->tokens);
  v_u64_destroy(&mod->lines);
  v_gab_constant_destroy(&mod->constants);
  v_s_i8_destroy(&mod->sources);

  DESTROY(mod);
}

gab_mod *gab_mod_copy(gab_eg *gab, gab_mod *self) {

  gab_mod *copy = NEW(gab_mod);

  copy->name = self->name;
  copy->source = gab_srccpy(gab, self->source);
  copy->previous_compiled_op = OP_NOP;

  copy->next = gab->modules;
  gab->modules = copy;

  v_u8_copy(&copy->bytecode, &self->bytecode);
  v_u8_copy(&copy->tokens, &self->tokens);
  v_u64_copy(&copy->lines, &self->lines);
  v_gab_constant_copy(&copy->constants, &self->constants);
  v_s_i8_copy(&copy->sources, &self->sources);

  // Reconcile the constant array by copying the non trivial values
  for (u64 i = 0; i < copy->constants.len; i++) {
    gab_value v = v_gab_constant_val_at(&self->constants, i);
    if (gab_valiso(v)) {
      v_gab_constant_set(&copy->constants, i, gab_valcpy(gab, NULL, v));
    }
  }

  // Reconcile the copied slices to point to the new source
  for (u64 i = 0; i < copy->sources.len; i++) {
    s_i8 *copy_src = v_s_i8_ref_at(&copy->sources, i);
    s_i8 *src_src = v_s_i8_ref_at(&self->sources, i);

    copy_src->data = copy->source->source->data +
                     (src_src->data - self->source->source->data);
  }

  return copy;
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
void gab_mod_push_op(gab_mod *self, gab_opcode op, gab_token t, u64 l, s_i8 s) {
  self->previous_compiled_op = op;
  v_u8_push(&self->bytecode, op);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
  v_s_i8_push(&self->sources, s);
};

void replace_previous_op(gab_mod *self, gab_opcode op, u8 args) {
  v_u8_set(&self->bytecode, self->bytecode.len - 1 - args, op);
  self->previous_compiled_op = op;
};

void gab_mod_push_byte(gab_mod *self, u8 data, gab_token t, u64 l, s_i8 s) {
  v_u8_push(&self->bytecode, data);
  v_u8_push(&self->tokens, t);
  v_u64_push(&self->lines, l);
  v_s_i8_push(&self->sources, s);
}

void gab_mod_push_short(gab_mod *self, u16 data, gab_token t, u64 l, s_i8 s) {
  gab_mod_push_byte(self, (data >> 8) & 0xff, t, l, s);
  gab_mod_push_byte(self, data & 0xff, t, l, s);
  v_s_i8_push(&self->sources, s);
};

/* These helpers return the instruction they push. */
void gab_mod_push_load_local(gab_mod *self, u8 local, gab_token t, u64 l,
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
    gab_mod_push_op(self, op, t, l, s);
    return;
  }

  gab_mod_push_op(self, OP_LOAD_LOCAL, t, l, s);
  gab_mod_push_byte(self, local, t, l, s);
}

void gab_mod_push_load_upvalue(gab_mod *self, u8 upvalue, gab_token t, u64 l,
                               s_i8 s) {
  if (upvalue < 9) {
    u8 op = MAKE_LOAD_UPVALUE(upvalue);
    gab_mod_push_op(self, op, t, l, s);
    return;
  }

  gab_mod_push_op(self, OP_LOAD_UPVALUE, t, l, s);
  gab_mod_push_byte(self, upvalue, t, l, s);
};

void gab_mod_push_store_local(gab_mod *self, u8 local, gab_token t, u64 l,
                              s_i8 s) {
  if (local < 9) {
    u8 op = MAKE_STORE_LOCAL(local);
    gab_mod_push_op(self, op, t, l, s);
    return;
  }

  gab_mod_push_op(self, OP_STORE_LOCAL, t, l, s);
  gab_mod_push_byte(self, local, t, l, s);
};

void gab_mod_push_return(gab_mod *self, u8 have, boolean mv, gab_token t, u64 l,
                         s_i8 s) {
  assert(have < 16);

  gab_mod_push_op(self, OP_RETURN, t, l, s);
  gab_mod_push_byte(self, (have << 1) | mv, t, l, s);
}

void gab_mod_push_tuple(gab_mod *self, u8 have, boolean mv, gab_token t, u64 l,
                        s_i8 s) {
  assert(have < 128);

  gab_mod_push_op(self, OP_TUPLE, t, l, s);
  gab_mod_push_byte(self, (have << 1) | mv, t, l, s);
}

void gab_mod_push_yield(gab_mod *self, u16 proto, u8 have, boolean mv,
                        gab_token t, u64 l, s_i8 s) {
  assert(have < 16);

  gab_mod_push_op(self, OP_YIELD, t, l, s);
  gab_mod_push_short(self, proto, t, l, s);
  gab_mod_push_byte(self, (have << 1) | mv, t, l, s);
}

void gab_mod_push_send(gab_mod *self, u8 have, u16 message, boolean mv,
                       gab_token t, u64 l, s_i8 s) {
  assert(have < 16);

  gab_mod_push_op(self, OP_SEND_ANA, t, l, s);
  gab_mod_push_short(self, message, t, l, s);
  gab_mod_push_byte(self, (have << 1) | mv, t, l, s);
  gab_mod_push_byte(self, 1, t, l, s);

  gab_mod_push_byte(self, OP_NOP, t, l, s); // Version
  gab_mod_push_inline_cache(self, t, l, s);
}

void gab_mod_push_pop(gab_mod *self, u8 n, gab_token t, u64 l, s_i8 s) {
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
    case OP_CONSTANT:
      self->bytecode.len -= 2;
    case OP_PUSH_FALSE:
    case OP_PUSH_TRUE:
    case OP_PUSH_UNDEFINED:
    case OP_PUSH_NIL:
    case OP_DUP:
      self->bytecode.len--;
      return;
    }

    gab_mod_push_op(self, op, t, l, s);
    return;
  }

  gab_mod_push_op(self, OP_POP_N, t, l, s);
  gab_mod_push_byte(self, n, t, l, s);
}

void gab_mod_push_inline_cache(gab_mod *self, gab_token t, u64 l, s_i8 s) {
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);

  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
}

void gab_mod_push_next(gab_mod *self, u8 next, gab_token t, u64 l, s_i8 s) {
  gab_mod_push_op(self, OP_NEXT, t, l, s);
  gab_mod_push_byte(self, next, t, l, s);
}

void gab_mod_push_pack(gab_mod *self, u8 below, u8 above, gab_token t, u64 l,
                       s_i8 s) {
  gab_mod_push_op(self, OP_PACK, t, l, s);
  gab_mod_push_byte(self, below, t, l, s);
  gab_mod_push_byte(self, above, t, l, s);
}

u64 gab_mod_push_iter(gab_mod *self, u8 start, u8 want, gab_token t, u64 l,
                      s_i8 s) {
  gab_mod_push_op(self, OP_ITER, t, l, s);
  gab_mod_push_byte(self, want, t, l, s);
  gab_mod_push_byte(self, start, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);

  return self->bytecode.len - 2;
}

u64 gab_mod_push_jump(gab_mod *self, u8 op, gab_token t, u64 l, s_i8 s) {
  gab_mod_push_op(self, op, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  gab_mod_push_byte(self, OP_NOP, t, l, s);
  return self->bytecode.len - 2;
}

void gab_mod_patch_jump(gab_mod *self, u64 jump) {
  u64 dist = self->bytecode.len - jump - 2;

  assert(dist < UINT16_MAX);

  v_u8_set(&self->bytecode, jump, (dist >> 8) & 0xff);
  v_u8_set(&self->bytecode, jump + 1, dist & 0xff);
}

u64 gab_mod_push_loop(gab_mod *self) { return self->bytecode.len; }

void gab_mod_patch_loop(gab_mod *self, u64 start, gab_token t, u64 l, s_i8 s) {
  u64 diff = self->bytecode.len - start + 3;
  assert(diff < UINT16_MAX);

  gab_mod_push_op(self, OP_LOOP, t, l, s);
  gab_mod_push_short(self, diff, t, l, s);
}

boolean gab_mod_try_patch_mv(gab_mod *self, u8 want) {
  switch (self->previous_compiled_op) {
  case OP_SEND_ANA:
    v_u8_set(&self->bytecode, self->bytecode.len - 18, want);
    return true;
  case OP_YIELD: {
    u16 offset =
        ((u16)v_u8_val_at(&self->bytecode, self->bytecode.len - 3) << 8) |
        v_u8_val_at(&self->bytecode, self->bytecode.len - 2);

    gab_value value = v_gab_constant_val_at(&self->constants, offset);

    assert(gab_valknd(value) == kGAB_SUSPENSE_PROTO);

    gab_obj_suspense_proto *proto = GAB_VAL_TO_SUSPENSE_PROTO(value);

    proto->want = want;
    return true;
  }
  }
  return false;
}

u16 gab_mod_add_constant(gab_mod *self, gab_value value) {
  if (self->constants.len >= GAB_CONSTANTS_MAX) {
    fprintf(stderr, "Uh oh, too many constants in the module.\n");
    exit(1);
  }
  v_gab_constant_push(&self->constants, value);
  return self->constants.len - 1;
};

u64 dumpInstruction(FILE *stream, gab_mod *self, u64 offset);

u64 dumpSimpleInstruction(FILE *stream, gab_mod *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s\n", name);
  return offset + 1;
}

u64 dumpSendInstruction(FILE *stream, gab_mod *self, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  u16 constant = ((u16)v_u8_val_at(&self->bytecode, offset + 1)) << 8 |
                 v_u8_val_at(&self->bytecode, offset + 2);

  gab_value msg = v_gab_constant_val_at(&self->constants, constant);

  u8 have = v_u8_val_at(&self->bytecode, offset + 3);
  u8 want = v_u8_val_at(&self->bytecode, offset + 4);

  u8 var = have & FLAG_VAR_EXP;
  ;
  have = have >> 1;

  fprintf(stream,
          "%-25s" ANSI_COLOR_BLUE "%-17V" ANSI_COLOR_RESET " (%s%d) -> %d\n",
          name, msg, var ? "VAR" : "", have, want);
  return offset + 22;
}

u64 dumpByteInstruction(FILE *stream, gab_mod *self, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}

u64 dumpTwoByteInstruction(FILE *stream, gab_mod *self, u64 offset) {
  u8 operandA = v_u8_val_at(&self->bytecode, offset + 1);
  u8 operandB = v_u8_val_at(&self->bytecode, offset + 2);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s%hhx %hhx\n", name, operandA, operandB);
  return offset + 3;
}

u64 dumpDictInstruction(FILE *stream, gab_mod *self, u8 i, u64 offset) {
  u8 operand = v_u8_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[i];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
};

u64 dumpConstantInstruction(FILE *stream, gab_mod *self, u64 offset) {
  u16 constant = ((u16)v_u8_val_at(&self->bytecode, offset + 1)) << 8 |
                 v_u8_val_at(&self->bytecode, offset + 2);
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s", name);
  gab_fdump(stdout, v_gab_constant_val_at(&self->constants, constant));
  fprintf(stream, "\n");
  return offset + 3;
}

u64 dumpJumpInstruction(FILE *stream, gab_mod *self, u64 sign, u64 offset) {
  const char *name = gab_opcode_names[v_u8_val_at(&self->bytecode, offset)];
  u16 dist = (u16)v_u8_val_at(&self->bytecode, offset + 1) << 8;
  dist |= v_u8_val_at(&self->bytecode, offset + 2);

  fprintf(stream,
          "%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET "\n",
          name, offset, offset + 3 + (sign * (dist)));
  return offset + 3;
}

u64 dumpIter(FILE *stream, gab_mod *self, u64 offset) {
  u16 dist = (u16)v_u8_val_at(&self->bytecode, offset + 3) << 8;
  dist |= v_u8_val_at(&self->bytecode, offset + 4);

  u8 nlocals = v_u8_val_at(&self->bytecode, offset + 1);
  u8 start = v_u8_val_at(&self->bytecode, offset + 2);

  fprintf(stream,
          "%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " %03d locals from %03d\n",
          "ITER", offset, offset + 5 + dist, nlocals, start);

  return offset + 5;
}

u64 dumpNext(FILE *stream, gab_mod *self, u64 offset) {
  fprintf(stream, "%-25s\n", "NEXT");
  return offset + 2;
}

u64 dumpInstruction(FILE *stream, gab_mod *self, u64 offset) {
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
  case OP_PUSH_FALSE:
  case OP_PUSH_NIL:
  case OP_PUSH_TRUE:
  case OP_PUSH_UNDEFINED:
  case OP_SWAP:
  case OP_DUP:
  case OP_NOT:
  case OP_MATCH:
  case OP_POP:
  case OP_TYPE:
  case OP_NOP:
    return dumpSimpleInstruction(stream, self, offset);
  case OP_PACK:
    return dumpTwoByteInstruction(stream, self, offset);
  case OP_LOGICAL_AND:
  case OP_JUMP_IF_FALSE:
  case OP_JUMP_IF_TRUE:
  case OP_JUMP:
  case OP_POP_JUMP_IF_FALSE:
  case OP_POP_JUMP_IF_TRUE:
  case OP_LOGICAL_OR:
    return dumpJumpInstruction(stream, self, 1, offset);
  case OP_LOOP:
    return dumpJumpInstruction(stream, self, -1, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(stream, self, offset);
  case OP_ITER:
    return dumpIter(stream, self, offset);
  case OP_STORE_PROPERTY_ANA:
  case OP_STORE_PROPERTY_MONO:
  case OP_STORE_PROPERTY_POLY:
  case OP_LOAD_PROPERTY_ANA:
  case OP_LOAD_PROPERTY_MONO:
  case OP_LOAD_PROPERTY_POLY:
    return dumpConstantInstruction(stream, self, offset) + 16;
  case OP_SEND_ANA:
  case OP_SEND_MONO_CLOSURE:
  case OP_SEND_MONO_BUILTIN:
  case OP_SEND_PRIMITIVE_STORE:
  case OP_SEND_PRIMITIVE_LOAD:
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
  case OP_SEND_PRIMITIVE_CALL_BLOCK:
  case OP_SEND_PRIMITIVE_CALL_BUILTIN:
  case OP_SEND_PRIMITIVE_CALL_SUSPENSE:
    return dumpSendInstruction(stream, self, offset);
  case OP_RETURN:
  case OP_YIELD:
  case OP_POP_N:
  case OP_STORE_LOCAL:
  case OP_POP_STORE_LOCAL:
  case OP_LOAD_UPVALUE:
  case OP_INTERPOLATE:
  case OP_DROP:
  case OP_SHIFT:
  case OP_NEXT:
  case OP_VAR:
  case OP_LOAD_LOCAL: {
    return dumpByteInstruction(stream, self, offset);
  }
  case OP_MESSAGE: {
    offset++;
    u16 proto_constant = ((((u16)self->bytecode.data[offset]) << 8) |
                          self->bytecode.data[offset + 1]);
    offset += 4;

    gab_value pval = v_gab_constant_val_at(&self->constants, proto_constant);
    gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(pval);

    s_i8 func_name = gab_obj_string_ref(GAB_VAL_TO_STRING(
        v_gab_constant_val_at(&p->mod->constants, p->mod->name)));

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_MESSAGE",
           (int)func_name.len, func_name.data);

    for (int j = 0; j < p->nupvalues; j++) {
      u8 flags = p->upv_desc[j * 2];
      u8 index = p->upv_desc[j * 2 + 1];
      int isLocal = flags & fLOCAL;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_BLOCK: {
    offset++;
    u16 proto_constant = ((((u16)self->bytecode.data[offset]) << 8) |
                          self->bytecode.data[offset + 1]);
    offset += 2;

    gab_value pval = v_gab_constant_val_at(&self->constants, proto_constant);
    gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(pval);

    s_i8 func_name = gab_obj_string_ref(GAB_VAL_TO_STRING(
        v_gab_constant_val_at(&p->mod->constants, p->mod->name)));

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_BLOCK",
           (int)func_name.len, func_name.data);

    for (int j = 0; j < p->nupvalues; j++) {
      u8 flags = p->upv_desc[j * 2];
      u8 index = p->upv_desc[j * 2 + 1];
      int isLocal = flags & fLOCAL;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_RECORD: {
    return dumpDictInstruction(stream, self, op, offset);
  }
  case OP_TUPLE: {
    return dumpDictInstruction(stream, self, op, offset);
  }
  default: {
    u8 code = v_u8_val_at(&self->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

void gab_fdis(FILE *stream, gab_mod *mod) {
  u64 offset = 0;
  while (offset < mod->bytecode.len) {
    fprintf(stream, ANSI_COLOR_YELLOW "%04lu " ANSI_COLOR_RESET, offset);
    offset = dumpInstruction(stream, mod, offset);
  }
}

void gab_mod_dump(gab_mod *self, s_i8 name) {
  printf("     %.*s\n", (int)name.len, name.data);
  gab_fdis(stdout, self);
}
