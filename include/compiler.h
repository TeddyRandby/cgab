#ifndef GAB_COMPILER_H
#define GAB_COMPILER_H

#include "include/core.h"
#include "lexer.h"
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
  GAB_VARIABLE_FLAG_CAPTURED = 1 << 0,
  GAB_VARIABLE_FLAG_MUTABLE = 1 << 1,
  GAB_VARIABLE_FLAG_LOCAL = 1 << 2,
} gab_variable_flag;

typedef struct gab_bc_frame gab_bc_frame;
struct gab_bc_frame {
  gab_module *mod;

  /*
    Scope keeps track of local variables, prevents name collisions,
    and pops variables off the stack when they go out of scope.

    Functions always create new scopes.
    If and for statements also create new scopes.
  */
  i32 scope_depth;

  u8 next_slot;
  u8 nslots;

  u8 next_local;
  u8 nlocals;

  u8 nupvalues;

  u8 locals_flag[LOCAL_MAX];
  i32 locals_depth[LOCAL_MAX];
  gab_value locals_name[LOCAL_MAX];

  u8 upvs_flag[UPVALUE_MAX];
  u8 upvs_index[UPVALUE_MAX];
};

/*
  State for compiling source code to a gab module.
*/
typedef struct gab_bc {
  gab_lexer lex;

  u64 line;

  /*
    State for lexing source code into tokens.
  */
  u8 flags;

  boolean panic;

  gab_token current_token;

  gab_token previous_token;

  u8 previous_op;

  u8 nimpls;

  u8 impls[FUNCTION_DEF_NESTING_MAX];

  /**
   * The number of compile frames
   */
  u8 nframe;

  /*
    Static array of compile frames.

    The max is an arbitrary chosen number for the maximum function nesting.
  */
  gab_bc_frame frames[FUNCTION_DEF_NESTING_MAX];
} gab_bc;

void gab_bc_create(gab_bc *bc, gab_source *source, u8 flags);

gab_value gab_bc_compile_send(gab_engine *gab, gab_value msg,
                              gab_value receiver, u8 flags, u8 narguments,
                              gab_value arguments[narguments]);

gab_value gab_bc_compile(gab_engine *gab, gab_value name, s_i8 source, u8 flags,
                         u8 narguments, gab_value arguments[narguments]);
#endif
