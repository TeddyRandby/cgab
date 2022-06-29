#ifndef GAB_MODULE_H
#define GAB_MODULE_H
#include "../lexer/lexer.h"
#include "../vm/gc.h"

typedef struct gab_module gab_module;

/*
  The bytecode of the vm.
*/
typedef enum gab_opcode {
#define OP_CODE(name) OP_##name,
#include "bytecode.h"
#undef OP_CODE
} gab_opcode;

static const char *gab_opcode_names[] = {
#define OP_CODE(name) #name,
#include "bytecode.h"
#undef OP_CODE
};

/*
  State required to run a gab program.
*/
struct gab_module {
  /*
    The name of the module
  */
  s_u8_ref name;

  /*
    The instructions, a contiguous vector of single-byte op-codes and args.
  */
  v_u8 bytecode;

  /* A sister vector to 'bytecode'.
     This vector relates each instruction to a line in the source code.
  */
  v_u64 lines;

  /*
    A sister vector to 'bytecode'.
    This vector relates each instruction to a token.
  */
  v_u8 tokens;

  /*
     A vector of each line of source code.
  */
  v_s_u8_ref *source_lines;

  /*
     A pointer to the string of source code.
  */
  s_u8 *source;

  /*
    The running engine.
  */
  gab_engine *engine;

  /*
    The next module in the linked list of modules.
  */
  gab_module *next;
};

/*
  Creating and destroying modules, from nothing and from a base module.
*/
gab_module *gab_module_create(gab_module *self, s_u8_ref name, s_u8 *source);

void gab_module_destroy(gab_module *self);

/*
   Push a byte and its metadata into the module.
*/
void gab_module_push_byte(gab_module *self, gab_opcode op, gab_token token,
                          u64 line);

/*
  A debug function for printing the instructions in a module.

  Defined in common/log.c
*/
void gab_module_dump(gab_module *self, s_u8_ref name);

void gab_module_optimize(gab_module *self);
#endif
