#include "../gab/compiler/compiler.h"
#include "../gab/vm/vm.h"

#include <stdarg.h>
#include <stdio.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

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
  return offset + 2;
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
  gab_val_dump(d_u64_index_key(&self->engine->constants, constant));
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
  case OP_STORE_LOCAL:
  case OP_STORE_UPVALUE:
  case OP_POP_STORE_LOCAL:
  case OP_POP_STORE_UPVALUE:
  case OP_LOAD_UPVALUE:
  case OP_LOAD_LOCAL: {
    return dumpByteInstruction(self, offset);
  }
  case OP_CLOSURE: {
    u16 constant = ((((u16)self->bytecode.data[offset + 1]) << 8) |
                    self->bytecode.data[offset + 2]);
    printf("%-16s ", "OP_CLOSURE");
    gab_val_dump(v_u64_val_at(&self->engine->constants.keys, constant));
    printf("\n");
    offset += 3;

    gab_obj_function *function = GAB_VAL_TO_FUNCTION(
        v_u64_val_at(&self->engine->constants.keys, constant));

    for (int j = 0; j < function->nupvalues; j++) {
      int isLocal = self->bytecode.data[offset++];
      int index = self->bytecode.data[offset++];
      printf("%04lu      |                     %s %d\n", offset - 2,
             isLocal ? "local" : "upvalue", index);
    }
    return offset;
  }
  case OP_OBJECT: {
    return dumpDictInstruction(self, offset);
  }
  default: {
    u8 code = v_u8_val_at(&self->bytecode, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

void gab_module_dump(gab_module *self, s_u8_ref name) {
  if (self) {
    printf("     %.*s\n", (int)name.size, name.data);
    u64 offset = 0;
    while (offset < self->bytecode.size) {
      printf("%04lu ", offset);
      offset = dumpInstruction(self, offset);
    }
  }
}

void gab_obj_dump(gab_value value) {
  switch (GAB_VAL_TO_OBJ(value)->kind) {
  case OBJECT_STRING: {
    gab_obj_string *obj = GAB_VAL_TO_STRING(value);
    printf("%s", (const char *)obj->data);
    break;
  }
  case OBJECT_FUNCTION: {
    gab_obj_function *obj = GAB_VAL_TO_FUNCTION(value);
    s_u8 *name = s_u8_create_sref(obj->name);
    printf("<function %s>", name->data);
    s_u8_destroy(name);
    break;
  }
  case OBJECT_SHAPE: {
    gab_obj_shape *shape = GAB_VAL_TO_SHAPE(value);
    printf("<shape %p>", shape);
    break;
  }
  case OBJECT_CLOSURE: {
    gab_obj_closure *obj = GAB_VAL_TO_CLOSURE(value);
    s_u8 *name = s_u8_create_sref(obj->func->name);
    printf("<closure %s>", name->data);
    s_u8_destroy(name);
    break;
  }
  case OBJECT_UPVALUE: {
    gab_obj_upvalue *upv = GAB_VAL_TO_UPVALUE(value);
    printf("upvalue [");
    gab_val_dump(*upv->data);
    printf("]");
    break;
  }
  case OBJECT_OBJECT: {
    gab_obj_object *obj = GAB_VAL_TO_OBJECT(value);
    printf("<object %p>", obj);
    break;
  }
  case OBJECT_BUILTIN: {
    gab_obj_builtin *obj = GAB_VAL_TO_BUILTIN(value);
    printf("<builtin %p>", obj);
    break;
  }
  }
}

void gab_val_dump(gab_value self) {
  if (GAB_VAL_IS_BOOLEAN(self)) {
    printf("%s", GAB_VAL_TO_BOOLEAN(self) ? "true" : "false");
  } else if (GAB_VAL_IS_NUMBER(self)) {
    printf("%g", GAB_VAL_TO_NUMBER(self));
  } else if (GAB_VAL_IS_NULL(self)) {
    printf("null");
  } else if (GAB_VAL_IS_OBJ(self)) {
    gab_obj_dump(self);
  }
}

void dump_compile_error(gab_compiler *compiler, const char *msg) {
  gab_compile_frame *frame = &compiler->frames[compiler->frame_count - 1];

  s_u8_ref curr_token = compiler->lex.previous_token_src;

  u64 curr_src_index = compiler->line - 1;

  s_u8_ref curr_src;
  if (curr_src_index < compiler->lex.source_lines->size) {
    curr_src = v_s_u8_ref_val_at(compiler->lex.source_lines, curr_src_index);
  } else {
    curr_src = s_u8_ref_create_cstr("");
  }

  s_u8 *func_name = s_u8_create_sref(frame->locals_name[0]);

  s_u8 *curr_line = s_u8_create_sref(curr_src);
  s_u8 *curr_under = s_u8_create_empty(curr_src.size);

  u8 *tok_start, *tok_end;
  tok_start = curr_token.data;
  tok_end = curr_token.data + curr_token.size;
  const char *tok = gab_token_names[compiler->previous_token];

  for (u8 i = 0; i < curr_under->size; i++) {
    if (curr_src.data + i >= tok_start && curr_src.data + i < tok_end)
      curr_under->data[i] = '^';
    else
      curr_under->data[i] = ' ';
  }

  const char *prev_color = ANSI_COLOR_GREEN;
  const char *curr_color = ANSI_COLOR_RED;

  const char *curr_box = "\u256d";

  fprintf(stderr,
          "[" ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "] line " ANSI_COLOR_RED
          "%04lu" ANSI_COLOR_RESET ": Error near " ANSI_COLOR_BLUE
          "%s" ANSI_COLOR_RESET "\n\t%s%s %.4lu " ANSI_COLOR_RESET "%s"
          "\n\t\u2502      " ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET
          "\n\t\u2570\u2500> ",
          func_name->data, compiler->line, tok, curr_box, curr_color,
          curr_src_index + 1, curr_line->data, curr_under->data);

  s_u8_destroy(func_name);
  s_u8_destroy(curr_line);
  s_u8_destroy(curr_under);

  fprintf(stderr, ANSI_COLOR_YELLOW "%s.\n" ANSI_COLOR_RESET, msg);
}

void dump_run_error(gab_vm *vm, const char *msg) {

  gab_call_frame *frame = vm->call_stack + 1;

  while (frame <= vm->frame) {
    gab_module *mod = frame->closure->func->module;

    s_u8 *func_name = s_u8_create_sref(frame->closure->func->name);

    u64 offset = frame->ip - mod->bytecode.data - 1;

    u64 curr_row = v_u64_val_at(&mod->lines, offset);

    // Row numbers start at one, so the index is one less.
    s_u8_ref curr_src = v_s_u8_ref_val_at(mod->source_lines, curr_row - 1);

    s_u8 *curr_line = s_u8_create_sref(curr_src);

    const char *tok = gab_token_names[v_u8_val_at(&mod->tokens, offset)];

    if (frame == vm->frame) {
      fprintf(stderr,
              "[" ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET
              "] line: " ANSI_COLOR_RED "%04lu" ANSI_COLOR_RESET
              " Error near " ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET
              "\n\t\u256d " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%s"
              "\n\t\u2502"
              "\n\t\u2570\u2500> ",
              func_name->data, curr_row, tok, curr_row, curr_line->data);

      fprintf(stderr, ANSI_COLOR_YELLOW "%s.\n" ANSI_COLOR_RESET, msg);
    } else {
      fprintf(stderr,
              "[" ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET
              "] line: " ANSI_COLOR_RED "%04lu" ANSI_COLOR_RESET " Called from"
              "\n\t  " ANSI_COLOR_RED "%04lu " ANSI_COLOR_RESET "%s"
              "\n\t"
              "\n",
              func_name->data, curr_row, curr_row, curr_line->data);
    }

    s_u8_destroy(func_name);
    frame++;
  }
}

boolean gab_result_has_error(gab_result *self) {
  return self->type == RESULT_COMPILE_FAIL || self->type == RESULT_RUN_FAIL;
};

void gab_result_dump_error(gab_result *self) {
  if (self->type == RESULT_COMPILE_FAIL) {
    dump_compile_error(self->as.compile_fail.compiler,
                       self->as.compile_fail.msg);
  } else if (self->type == RESULT_RUN_FAIL) {
    dump_run_error(self->as.run_fail.vm, self->as.run_fail.msg);
  }
};

gab_result *gab_compile_fail(gab_compiler *compiler, const char *msg) {
  gab_result *self = CREATE_STRUCT(gab_result);
  self->type = RESULT_COMPILE_FAIL;

  // Copy the compiler's state at the point of the error.
  self->as.compile_fail.compiler = CREATE_STRUCT(gab_compiler);
  COPY(self->as.compile_fail.compiler, compiler, sizeof(gab_compiler));

  self->as.compile_fail.msg = msg;

  return self;
};

gab_result *gab_compile_success(gab_obj_closure *main) {
  gab_result *self = CREATE_STRUCT(gab_result);
  self->type = RESULT_COMPILE_SUCCESS;
  self->as.main = main;
  return self;
};

gab_result *gab_run_fail(gab_vm *vm, const char *msg) {
  gab_result *self = CREATE_STRUCT(gab_result);
  self->type = RESULT_RUN_FAIL;

  self->as.run_fail.vm = CREATE_STRUCT(gab_vm);

  COPY(self->as.run_fail.vm, vm, sizeof(gab_vm));

  self->as.run_fail.vm->frame =
      self->as.run_fail.vm->call_stack + (vm->frame - vm->call_stack);

  self->as.run_fail.msg = msg;
  return self;
};

gab_result *gab_run_success(gab_value data) {
  gab_result *self = CREATE_STRUCT(gab_result);

  self->type = RESULT_RUN_SUCCESS;
  self->as.result = data;
  return self;
};

void gab_result_destroy(gab_result *self) {
  switch (self->type) {
  case RESULT_RUN_FAIL:
    DESTROY_STRUCT(self->as.run_fail.vm);
    break;
  case RESULT_COMPILE_FAIL:
    DESTROY_STRUCT(self->as.compile_fail.compiler);
    break;
  default:
    break;
  }

  DESTROY_STRUCT(self);
};
