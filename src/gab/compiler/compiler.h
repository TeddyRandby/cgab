#ifndef BLUF_COMPILER_H
#define BLUF_COMPILER_H

#include "../lexer/lexer.h"
#include "module.h"

/*
  A compile frame is the compile-time equivalent of a call frame.
  They are pushed and popped as functions are compiled,
  And they keep track of locals and upvalues.

  Locals and upvalues are just indexes into these arrays - just like
  they are at runtime. Since the size of an instruction is a single byte,
  an index into the local array at runtime is limited to
  a maximum of 255, or UINT8_MAX.

  Therefore the maximum number of locals/upvalues in a function is
  256, or UINT8_COUNT.
*/
typedef struct gab_compile_frame gab_compile_frame;
struct gab_compile_frame {

  u16 local_count;

  u16 deepest_local;

  u16 upv_count;

  s_u8_ref function_name;

  s_u8_ref locals_name[UINT8_COUNT];
  i32 locals_depth[UINT8_COUNT];
  boolean locals_captured[UINT8_COUNT];

  u8 upvs_index[UINT8_COUNT];
  boolean upvs_is_local[UINT8_COUNT];
};

/*
  State for compiling source code to a gab module.
*/
typedef struct gab_compiler gab_compiler;
struct gab_compiler {
  /*
    State for lexing source code into tokens.
  */
  gab_token current_token;
  gab_token previous_token;
  u8 previous_op;
  gab_lexer lex;
  u64 line;

  /*
    If the compiler encounters an error, panic_mode will
    be turned on, until it is turned off by a statement.
  */
  const char *error;

  /*
    The module where bytecode is being compiled into.
  */
  gab_module *mod;

  /*
    Scope keeps track of local variables, prevents name collisions,
    and pops variables off the stack when they go out of scope.

    Functions always create new scopes.
    If and for statements also create new scopes.
  */
  i32 scope_depth;

  /*
    Static array of compile frames.

    The max is an arbitrary chosen number for the maximum function nesting.
  */
  gab_compile_frame frames[FUNCTION_DEF_NESTING_MAX];
  u8 frame_count;
};

/*
  Precedence rules for the parsing of expressions.
*/
typedef enum gab_precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_BITWISE_OR,  // |
  PREC_BITWISE_AND, // &
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // (
  PREC_PROPERTY,    // . ->
  PREC_PRIMARY
} gab_precedence;

/*
  Compile rules used for Pratt parsing of expressions.
*/
typedef i32 (*gab_compile_func)(gab_compiler *, boolean assignable);
typedef struct gab_compile_rule gab_compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  gab_compile_func postfix;
  gab_precedence prec;
  boolean multi_line;
};
#endif
