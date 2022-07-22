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

typedef enum gab_variable_flag {
  FLAG_CAPTURED = 1 << 0,
  FLAG_MUTABLE = 1 << 1,
  FLAG_LOCAL = 1 << 2,
} gab_variable_flag;

typedef struct gab_compile_frame gab_compile_frame;
struct gab_compile_frame {

  u16 deepest_local;

  u16 local_count;

  u16 upv_count;

  s_u8_ref function_name;

  s_u8_ref locals_name[LOCAL_MAX];
  i32 locals_depth[LOCAL_MAX];
  u8 locals_flag[LOCAL_MAX];

  u8 upvs_index[UPVALUE_MAX];
  u8 upvs_flag[UPVALUE_MAX];
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

#endif
