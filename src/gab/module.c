#include "include/module.h"
#include "include/colors.h"
#include "include/compiler.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/lexer.h"
#include <stdint.h>
#include <stdio.h>

struct gab_mod *gab_mod(struct gab_eg *eg, gab_value name,
                        struct gab_src *source) {
  struct gab_mod *self = NEW(struct gab_mod);
  memset(self, 0, sizeof(struct gab_mod));

  self->name = name;
  self->source = source;
  self->next = eg->modules;
  eg->modules = self;

  return self;
}

void gab_moddestroy(struct gab_triple gab, struct gab_mod *mod) {
  if (!mod)
    return;

  gab_ngcdref(gab, 1, mod->constants.len, mod->constants.data);

  v_uint8_t_destroy(&mod->bytecode);
  v_uint64_t_destroy(&mod->bytecode_toks);
  v_gab_constant_destroy(&mod->constants);

  DESTROY(mod);
}

struct gab_mod *gab_modcpy(struct gab_triple gab, struct gab_mod *self) {
  struct gab_mod *copy = NEW(struct gab_mod);

  copy->name = self->name;
  copy->source = gab_srccpy(gab.eg, self->source);
  copy->previous_compiled_op = OP_NOP;

  copy->next = gab.eg->modules;
  gab.eg->modules = copy;

  v_uint8_t_copy(&copy->bytecode, &self->bytecode);
  v_uint64_t_copy(&copy->bytecode_toks, &self->bytecode_toks);
  v_gab_constant_copy(&copy->constants, &self->constants);

  // Reconcile the constant array by copying the non trivial values
  for (size_t i = 0; i < copy->constants.len; i++) {
    gab_value v = v_gab_constant_val_at(&self->constants, i);
    if (gab_valiso(v)) {
      v_gab_constant_set(&copy->constants, i, gab_valcpy(gab, v));
    }
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
void gab_mod_push_op(struct gab_mod *self, gab_opcode op, size_t t) {
  self->previous_compiled_op = op;
  v_uint8_t_push(&self->bytecode, op);
  v_uint64_t_push(&self->bytecode_toks, t);
};

void gab_mod_push_byte(struct gab_mod *self, uint8_t data, size_t t) {
  v_uint8_t_push(&self->bytecode, data);
  v_uint64_t_push(&self->bytecode_toks, t);
}

void gab_mod_push_short(struct gab_mod *self, uint16_t data, size_t t) {
  gab_mod_push_byte(self, (data >> 8) & 0xff, t);
  gab_mod_push_byte(self, data & 0xff, t);
};

/* These helpers return the instruction they push. */
void gab_mod_push_load_local(struct gab_mod *self, uint8_t local, size_t t) {
  if (local < 9) {
    uint8_t op = MAKE_LOAD_LOCAL(local);
    gab_mod_push_op(self, op, t);
    return;
  }

  gab_mod_push_op(self, OP_LOAD_LOCAL, t);
  gab_mod_push_byte(self, local, t);
}

void gab_mod_push_load_upvalue(struct gab_mod *self, uint8_t upvalue,
                               size_t t) {
  if (upvalue < 9) {
    uint8_t op = MAKE_LOAD_UPVALUE(upvalue);
    gab_mod_push_op(self, op, t);
    return;
  }

  gab_mod_push_op(self, OP_LOAD_UPVALUE, t);
  gab_mod_push_byte(self, upvalue, t);
};

void gab_mod_push_store_local(struct gab_mod *self, uint8_t local, size_t t) {
  if (local < 9) {
    uint8_t op = MAKE_STORE_LOCAL(local);
    gab_mod_push_op(self, op, t);
    return;
  }

  gab_mod_push_op(self, OP_STORE_LOCAL, t);
  gab_mod_push_byte(self, local, t);
};

void gab_mod_push_return(struct gab_mod *self, uint8_t have, bool mv,
                         size_t t) {
  assert(have < 16);

  gab_mod_push_op(self, OP_RETURN, t);
  gab_mod_push_byte(self, (have << 1) | mv, t);
}

void gab_mod_push_tuple(struct gab_mod *self, uint8_t have, bool mv, size_t t) {
  assert(have < 128);

  gab_mod_push_op(self, OP_TUPLE, t);
  gab_mod_push_byte(self, (have << 1) | mv, t);
}

void gab_mod_push_yield(struct gab_mod *self, uint16_t proto, uint8_t have,
                        bool mv, size_t t) {
  assert(have < 16);

  gab_mod_push_op(self, OP_YIELD, t);
  gab_mod_push_short(self, proto, t);
  gab_mod_push_byte(self, (have << 1) | mv, t);
}

void gab_mod_push_send(struct gab_mod *self, uint8_t have, uint16_t message,
                       bool mv, size_t t) {
  assert(have < 16);

  gab_mod_push_op(self, OP_SEND_ANA, t);
  gab_mod_push_short(self, message, t);
  gab_mod_push_byte(self, (have << 1) | mv, t);
  gab_mod_push_byte(self, 1, t);

  gab_mod_push_byte(self, OP_NOP, t); // Version
  gab_mod_push_inline_cache(self, t);
}

void gab_mod_push_pop(struct gab_mod *self, uint8_t n, size_t t) {
  if (n == 1) {
    gab_mod_push_op(self, OP_POP, t);
  } else {
    gab_mod_push_op(self, OP_POP_N, t);
    gab_mod_push_byte(self, n, t);
  }
}

void gab_mod_push_inline_cache(struct gab_mod *self, size_t t) {
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);

  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
}

void gab_mod_push_next(struct gab_mod *self, uint8_t next, size_t t) {
  gab_mod_push_op(self, OP_NEXT, t);
  gab_mod_push_byte(self, next, t);
}

void gab_mod_push_pack(struct gab_mod *self, uint8_t below, uint8_t above,
                       size_t t) {
  gab_mod_push_op(self, OP_PACK, t);
  gab_mod_push_byte(self, below, t);
  gab_mod_push_byte(self, above, t);
}

uint64_t gab_mod_push_iter(struct gab_mod *self, uint8_t start, uint8_t want,
                           size_t t) {
  gab_mod_push_op(self, OP_ITER, t);
  gab_mod_push_byte(self, want, t);
  gab_mod_push_byte(self, start, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);

  return self->bytecode.len - 2;
}

uint64_t gab_mod_push_jump(struct gab_mod *self, uint8_t op, size_t t) {
  gab_mod_push_op(self, op, t);
  gab_mod_push_byte(self, OP_NOP, t);
  gab_mod_push_byte(self, OP_NOP, t);
  return self->bytecode.len - 2;
}

void gab_mod_patch_jump(struct gab_mod *self, uint64_t jump) {
  uint64_t dist = self->bytecode.len - jump - 2;

  assert(dist < UINT16_MAX);

  v_uint8_t_set(&self->bytecode, jump, (dist >> 8) & 0xff);
  v_uint8_t_set(&self->bytecode, jump + 1, dist & 0xff);
}

uint64_t gab_mod_push_loop(struct gab_mod *self) { return self->bytecode.len; }

void gab_mod_patch_loop(struct gab_mod *self, uint64_t start, size_t t) {
  uint64_t diff = self->bytecode.len - start + 3;
  assert(diff < UINT16_MAX);

  gab_mod_push_op(self, OP_LOOP, t);
  gab_mod_push_short(self, diff, t);
}

bool gab_mod_try_patch_mv(struct gab_mod *self, uint8_t want) {
  switch (self->previous_compiled_op) {
  case OP_SEND_ANA:
    v_uint8_t_set(&self->bytecode, self->bytecode.len - 18, want);
    return true;
  case OP_YIELD: {
    uint16_t offset =
        ((uint16_t)v_uint8_t_val_at(&self->bytecode, self->bytecode.len - 3)
         << 8) |
        v_uint8_t_val_at(&self->bytecode, self->bytecode.len - 2);

    gab_value value = v_gab_constant_val_at(&self->constants, offset);

    assert(gab_valknd(value) == kGAB_SUSPENSE_PROTO);

    struct gab_obj_suspense_proto *proto = GAB_VAL_TO_SUSPENSE_PROTO(value);

    proto->want = want;
    return true;
  }
  }
  return false;
}

uint16_t gab_mod_add_constant(struct gab_mod *self, gab_value value) {
  if (self->constants.len >= GAB_CONSTANTS_MAX) {
    fprintf(stderr, "Uh oh, too many constants in the module.\n");
    exit(1);
  }
  v_gab_constant_push(&self->constants, value);
  return self->constants.len - 1;
};

uint64_t dumpInstruction(FILE *stream, struct gab_mod *self, uint64_t offset);

uint64_t dumpSimpleInstruction(FILE *stream, struct gab_mod *self,
                               uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s\n", name);
  return offset + 1;
}

uint64_t dumpSendInstruction(FILE *stream, struct gab_mod *self,
                             uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->bytecode, offset)];
  uint16_t constant = ((uint16_t)v_uint8_t_val_at(&self->bytecode, offset + 1))
                          << 8 |
                      v_uint8_t_val_at(&self->bytecode, offset + 2);

  gab_value msg = v_gab_constant_val_at(&self->constants, constant);

  uint8_t have = v_uint8_t_val_at(&self->bytecode, offset + 3);
  uint8_t want = v_uint8_t_val_at(&self->bytecode, offset + 4);

  uint8_t var = have & FLAG_VAR_EXP;
  ;
  have = have >> 1;

  fprintf(stream,
          "%-25s" ANSI_COLOR_BLUE "%-17V" ANSI_COLOR_RESET " (%s%d) -> %d\n",
          name, msg, var ? "VAR" : "", have, want);
  return offset + 22;
}

uint64_t dumpByteInstruction(FILE *stream, struct gab_mod *self,
                             uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->bytecode, offset + 1);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}

uint64_t dumpYieldInstruction(FILE *stream, struct gab_mod *self,
                              uint64_t offset) {
  uint8_t have = v_uint8_t_val_at(&self->bytecode, offset + 3);
  fprintf(stream, "%-25s%hhx\n", "YIELD", have);
  return offset + 4;
}

uint64_t dumpTwoByteInstruction(FILE *stream, struct gab_mod *self,
                                uint64_t offset) {
  uint8_t operandA = v_uint8_t_val_at(&self->bytecode, offset + 1);
  uint8_t operandB = v_uint8_t_val_at(&self->bytecode, offset + 2);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s%hhx %hhx\n", name, operandA, operandB);
  return offset + 3;
}

uint64_t dumpDictInstruction(FILE *stream, struct gab_mod *self, uint8_t i,
                             uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->bytecode, offset + 1);
  const char *name = gab_opcode_names[i];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
};

uint64_t dumpConstantInstruction(FILE *stream, struct gab_mod *self,
                                 uint64_t offset) {
  uint16_t constant = ((uint16_t)v_uint8_t_val_at(&self->bytecode, offset + 1))
                          << 8 |
                      v_uint8_t_val_at(&self->bytecode, offset + 2);
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->bytecode, offset)];
  fprintf(stream, "%-25s", name);
  gab_fvaldump(stdout, v_gab_constant_val_at(&self->constants, constant));
  fprintf(stream, "\n");
  return offset + 3;
}

uint64_t dumpJumpInstruction(FILE *stream, struct gab_mod *self, uint64_t sign,
                             uint64_t offset) {
  const char *name =
      gab_opcode_names[v_uint8_t_val_at(&self->bytecode, offset)];
  uint16_t dist = (uint16_t)v_uint8_t_val_at(&self->bytecode, offset + 1) << 8;
  dist |= v_uint8_t_val_at(&self->bytecode, offset + 2);

  fprintf(stream,
          "%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET "\n",
          name, offset, offset + 3 + (sign * (dist)));
  return offset + 3;
}

uint64_t dumpIter(FILE *stream, struct gab_mod *self, uint64_t offset) {
  uint16_t dist = (uint16_t)v_uint8_t_val_at(&self->bytecode, offset + 3) << 8;
  dist |= v_uint8_t_val_at(&self->bytecode, offset + 4);

  uint8_t nlocals = v_uint8_t_val_at(&self->bytecode, offset + 1);
  uint8_t start = v_uint8_t_val_at(&self->bytecode, offset + 2);

  fprintf(stream,
          "%-25s" ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " -> " ANSI_COLOR_YELLOW "%04lu" ANSI_COLOR_RESET
          " %03d locals from %03d\n",
          "ITER", offset, offset + 5 + dist, nlocals, start);

  return offset + 5;
}

uint64_t dumpNext(FILE *stream, struct gab_mod *self, uint64_t offset) {
  fprintf(stream, "%-25s\n", "NEXT");
  return offset + 2;
}

uint64_t dumpInstruction(FILE *stream, struct gab_mod *self, uint64_t offset) {
  uint8_t op = v_uint8_t_val_at(&self->bytecode, offset);
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
  case OP_SEND_ANA:
  case OP_SEND_MONO_CLOSURE:
  case OP_SEND_MONO_BUILTIN:
  case OP_SEND_PROPERTY:
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
  case OP_POP_N:
  case OP_STORE_LOCAL:
  case OP_POP_STORE_LOCAL:
  case OP_LOAD_UPVALUE:
  case OP_INTERPOLATE:
  case OP_SHIFT:
  case OP_NEXT:
  case OP_VAR:
  case OP_LOAD_LOCAL: {
    return dumpByteInstruction(stream, self, offset);
  }
  case OP_YIELD: {
    return dumpYieldInstruction(stream, self, offset);
  }
  case OP_MESSAGE: {
    offset++;
    uint16_t proto_constant = ((((uint16_t)self->bytecode.data[offset]) << 8) |
                               self->bytecode.data[offset + 1]);
    offset += 4;

    gab_value pval = v_gab_constant_val_at(&self->constants, proto_constant);
    struct gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(pval);

    struct gab_obj_string *func_name = GAB_VAL_TO_STRING(p->mod->name);

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_MESSAGE",
           (int)func_name->len, func_name->data);

    for (int j = 0; j < p->nupvalues; j++) {
      uint8_t flags = p->upv_desc[j * 2];
      uint8_t index = p->upv_desc[j * 2 + 1];
      int isLocal = flags & fVAR_LOCAL;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_BLOCK: {
    offset++;
    uint16_t proto_constant = ((((uint16_t)self->bytecode.data[offset]) << 8) |
                               self->bytecode.data[offset + 1]);
    offset += 2;

    gab_value pval = v_gab_constant_val_at(&self->constants, proto_constant);
    struct gab_obj_block_proto *p = GAB_VAL_TO_BLOCK_PROTO(pval);

    struct gab_obj_string *func_name = GAB_VAL_TO_STRING(p->mod->name);

    printf("%-25s" ANSI_COLOR_CYAN "%-20.*s\n" ANSI_COLOR_RESET, "OP_BLOCK",
           (int)func_name->len, func_name->data);

    for (int j = 0; j < p->nupvalues; j++) {
      uint8_t flags = p->upv_desc[j * 2];
      uint8_t index = p->upv_desc[j * 2 + 1];
      int isLocal = flags & fVAR_LOCAL;
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
    uint8_t code = v_uint8_t_val_at(&self->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

void gab_fmoddump(FILE *stream, struct gab_mod *mod) {
  uint64_t offset = 0;
  while (offset < mod->bytecode.len) {
    fprintf(stream, ANSI_COLOR_YELLOW "%04lu " ANSI_COLOR_RESET, offset);
    offset = dumpInstruction(stream, mod, offset);
  }
}

void gab_mod_dump(struct gab_mod *self, s_char name) {
  printf("     %.*s\n", (int)name.len, name.data);
  gab_fmoddump(stdout, self);
}
