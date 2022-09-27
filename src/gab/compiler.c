#include "include/compiler.h"
#include "include/engine.h"
#include "include/module.h"
#include "include/object.h"
#include "include/gab.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  PREC_UNARY,       // ! - not
  PREC_CALL,        // ( {
  PREC_PROPERTY,    // . :
  PREC_PRIMARY
} gab_precedence;

/*
  Compile rules used for Pratt parsing of expressions.
*/

typedef i32 (*gab_compile_func)(gab_engine *, boolean);

typedef struct gab_compile_rule gab_compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  gab_compile_func postfix;
  gab_precedence prec;
  boolean multi_line;
};

void gab_bc_create(gab_bc *self) {
  self->scope_depth = 0;
  self->frame_count = 0;
  self->panic = false;
  memset(self->frames, 0, sizeof(self->frames));
}

// A positive result is known to be OK, and can carry a value.
enum comp_status {
  COMP_OK = 1,
  COMP_TOKEN_NO_MATCH = 0,
  COMP_ERR = -1,
  COMP_LOCAL_NOT_FOUND = -2,
  COMP_UPVALUE_NOT_FOUND = -3,
  COMP_ID_NOT_FOUND = -4,
  COMP_RESOLVED_TO_LOCAL = -5,
  COMP_RESOLVED_TO_UPVALUE = -6,
  COMP_MAX = INT32_MAX,
};

static void write_error(gab_engine *gab, gab_error_k e, const char *help_fmt,
                        ...);

static boolean match_token(gab_engine *self, gab_token tok);

static i32 eat_token(gab_engine *self);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline boolean match_token(gab_engine *self, gab_token tok) {
  return self->bc.current_token == tok;
}

// Returns less than 0 if there was an error, greater than 0 otherwise.
static i32 eat_token(gab_engine *self) {
  self->bc.previous_token = self->bc.current_token;
  self->bc.current_token = gab_lexer_next(&self->bc.lex);
  self->bc.line = self->bc.lex.current_row;

  if (match_token(self, TOKEN_ERROR)) {
    eat_token(self);
    write_error(self, GAB_ERROR_MALFORMED_TOKEN, self->bc.lex.error_msg);
    return COMP_ERR;
  }

  return COMP_OK;
}

static i32 match_and_eat_token(gab_engine *self, gab_token tok) {
  if (!match_token(self, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(self);
}

static i32 expect_token(gab_engine *self, gab_token tok) {
  if (!match_token(self, tok)) {
    eat_token(self);
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN, "Expected %s",
                gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(self);
}

static inline u16 add_constant(gab_engine *self, gab_value value) {
  u16 index = gab_add_constant(self->bc.mod->engine, value);
  return index;
}

static inline void push_op(gab_engine *self, gab_opcode op) {
  gab_module_push_op(self->bc.mod, op, self->bc.previous_token, self->bc.line);
}

static inline void push_byte(gab_engine *self, u8 data) {
  gab_module_push_byte(self->bc.mod, data, self->bc.previous_token,
                       self->bc.line);
}

static inline void push_short(gab_engine *self, u16 data) {
  gab_module_push_short(self->bc.mod, data, self->bc.previous_token,
                        self->bc.line);
}

//--------------------- Local Variable Helpers --------------
static gab_bc_frame *peek_frame(gab_engine *self, u32 depth) {
  return &self->bc.frames[self->bc.frame_count - depth - 1];
}

static void initialize_local(gab_engine *self, u8 local) {
  peek_frame(self, 0)->locals_depth[local] = self->bc.scope_depth;
}

static i32 add_invisible_local(gab_engine *self) {
  gab_bc_frame *frame = peek_frame(self, 0);

  if (frame->local_count == LOCAL_MAX) {
    write_error(self, GAB_ERROR_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  u8 local = frame->local_count;

  frame->locals_name[local] = s_i8_create(NULL, 0);
  initialize_local(self, local);

  if (++frame->local_count > frame->deepest_local)
    frame->deepest_local = frame->local_count;

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static i32 add_local(gab_engine *self, s_i8 name, u8 flags) {
  gab_bc_frame *frame = peek_frame(self, 0);
  if (frame->local_count == LOCAL_MAX) {
    write_error(self, GAB_ERROR_TOO_MANY_LOCALS, "");
    return COMP_ERR;
  }

  u8 local = frame->local_count;
  frame->locals_name[local] = name;
  frame->locals_depth[local] = -1;
  frame->locals_flag[local] = flags;

  if (++frame->local_count > frame->deepest_local)
    frame->deepest_local = frame->local_count;

  return local;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * upvalue.
 */
static i32 add_upvalue(gab_engine *self, u32 depth, u8 index, u8 flags) {
  gab_bc_frame *frame = peek_frame(self, depth);
  u16 count = frame->upv_count;

  // Don't pull redundant upvalues
  for (int i = 0; i < count; i++) {
    if (frame->upvs_index[i] == index && (frame->upvs_flag[i]) == flags) {
      return i;
    }
  }

  if (count == UPVALUE_MAX) {
    write_error(self, GAB_ERROR_TOO_MANY_UPVALUES, "");
    return COMP_ERR;
  }

  frame->upvs_index[count] = index;
  frame->upvs_flag[count] = flags;
  frame->upv_count++;

  return count;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_LOCAL_NOT_FOUND if no matching local is found,
 * and otherwise the offset of the local.
 */
static i32 resolve_local(gab_engine *self, s_i8 name, u32 depth) {
  for (i32 local = peek_frame(self, depth)->local_count - 1; local >= 0;
       local--) {
    if (s_i8_match(name, peek_frame(self, depth)->locals_name[local])) {
      if (peek_frame(self, depth)->locals_depth[local] == -1) {
        write_error(self, GAB_ERROR_REFERENCE_BEFORE_INITIALIZE, "");
        return COMP_ERR;
      }
      return local;
    }
  }
  return COMP_LOCAL_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_UPVALUE_NOT_FOUND if no matching upvalue is found,
 * and otherwise the offset of the upvalue.
 */
static i32 resolve_upvalue(gab_engine *self, s_i8 name, u32 depth) {
  // Base case, hopefully conversion doesn't cause issues
  if (depth >= self->bc.frame_count - 1) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  i32 local = resolve_local(self, name, depth + 1);
  if (local >= 0) {
    u8 flags =
        (peek_frame(self, depth + 1)->locals_flag[local] |= FLAG_CAPTURED);
    return add_upvalue(self, depth, local, flags | FLAG_LOCAL);
  }

  i32 upvalue = resolve_upvalue(self, name, depth + 1);
  if (upvalue >= 0) {
    u8 flags = peek_frame(self, depth + 1)->upvs_flag[upvalue];
    return add_upvalue(self, depth, upvalue, flags ^ FLAG_LOCAL);
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_REGABVED_TO_LOCAL if the id was a local, and
 * COMP_REGABVED_TO_UPVALUE if the id was an upvalue.
 */
static i32 resolve_id(gab_engine *self, s_i8 name, u8 *value_in) {

  i32 arg = resolve_local(self, name, 0);

  if (arg == COMP_ERR)
    return arg;

  if (arg == COMP_LOCAL_NOT_FOUND) {

    arg = resolve_upvalue(self, name, 0);
    if (arg == COMP_ERR)
      return arg;

    if (arg == COMP_UPVALUE_NOT_FOUND)
      return COMP_ID_NOT_FOUND;

    *value_in = (u8)arg;
    return COMP_RESOLVED_TO_UPVALUE;
  }

  if (value_in)
    *value_in = (u8)arg;
  return COMP_RESOLVED_TO_LOCAL;
}

static void down_scope(gab_engine *self) { self->bc.scope_depth++; }

static void up_scope(gab_engine *self) {
  self->bc.scope_depth--;

  gab_bc_frame *frame = peek_frame(self, 0);

  boolean capture = false;
  while (frame->local_count > 1 &&
         frame->locals_depth[frame->local_count - 1] > self->bc.scope_depth) {
    capture |= frame->locals_flag[frame->local_count - 1] & FLAG_CAPTURED;
    frame->local_count--;
  }

  // If this block contained a captured local, escape it.
  if (capture) {
    gab_module_push_op(self->bc.mod, OP_CLOSE_UPVALUE, self->bc.previous_token,
                       self->bc.line);
    gab_module_push_byte(self->bc.mod, frame->local_count,
                         self->bc.previous_token, self->bc.line);
  }
}

static void down_frame(gab_engine *self, s_i8 name) {
  self->bc.frame_count++;
  // The first local in any given frame is reserved for the function being
  // called. It is immutable.
  peek_frame(self, 0)->local_count = 0;
  peek_frame(self, 0)->upv_count = 0;
  peek_frame(self, 0)->deepest_local = 0;
  peek_frame(self, 0)->function_name = name;

  i32 loc = add_local(self, name, 0);
  initialize_local(self, loc);
}

static void up_frame(gab_engine *self) {
  peek_frame(self, 0)->local_count = 0;
  peek_frame(self, 0)->upv_count = 0;
  self->bc.frame_count--;
  self->bc.scope_depth--;
}

// Forward declare some functions
gab_compile_rule get_rule(gab_token k);
i32 compile_exp_prec(gab_engine *self, gab_precedence prec);
i32 compile_expressions(gab_engine *self, u8 want, boolean *vse_out);
i32 compile_expression(gab_engine *self);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

/* Returns COMP_ERR if an error was encountered, and otherwise the offset of the
 * local.
 */
static i32 compile_local(gab_engine *self, s_i8 name, u8 flags) {

  for (i32 local = peek_frame(self, 0)->local_count - 1; local >= 0; local--) {
    if (peek_frame(self, 0)->locals_depth[local] != -1 &&
        peek_frame(self, 0)->locals_depth[local] < self->bc.scope_depth) {
      break;
    }

    if (s_i8_match(name, peek_frame(self, 0)->locals_name[local])) {
      write_error(self, GAB_ERROR_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }
  }

  return add_local(self, name, flags);
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
i32 compile_function_args(gab_engine *self) {

  i32 result = 0;
  u8 narguments = 0;

  do {

    if (narguments == FARG_MAX) {
      write_error(self, GAB_ERROR_TOO_MANY_PARAMETERS, "");
      return COMP_ERR;
    }

    narguments++;

    if (expect_token(self, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = self->bc.lex.previous_token_src;
    i32 local = compile_local(self, name, 0);
    if (local < 0)
      return COMP_ERR;

    // Function arguments are "initialized"
    // here as well.
    initialize_local(self, local);

  } while ((result = match_and_eat_token(self, TOKEN_COMMA)));

  if (result < 0)
    return COMP_ERR;

  return narguments;
}

static inline i32 skip_newlines(gab_engine *self) {
  while (match_token(self, TOKEN_NEWLINE)) {
    if (eat_token(self) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 optional_newline(gab_engine *self) {
  match_and_eat_token(self, TOKEN_NEWLINE);
  return COMP_OK;
}

i32 compile_block_expression(gab_engine *self) {
  i32 result = compile_expression(self);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(self, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  if (skip_newlines(self) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_block_body(gab_engine *self) {
  if (skip_newlines(self) < 0)
    return COMP_ERR;

  i32 result = compile_block_expression(self);

  if (result < 0)
    return COMP_ERR;

  while (!match_token(self, TOKEN_END) && !match_token(self, TOKEN_EOF)) {
    push_op(self, OP_POP);

    result = compile_block_expression(self);
    if (result < 0)
      return COMP_ERR;
  }

  return result;
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
i32 compile_function_body(gab_engine *self, gab_obj_function *function,
                          u64 skip_jump) {
  // Start compiling the function's bytecode.
  // You can't just store a pointer to the beginning of the code
  // Because if the vector is EVER resized,
  // Then all your functions are suddenly invalid.
  function->offset = self->bc.mod->bytecode.len;

  boolean vse;
  i32 result = compile_expressions(self, VAR_RET, &vse);

  if (result < 0)
    return COMP_ERR;

  gab_module_push_return(self->bc.mod, result, vse, self->bc.previous_token,
                         self->bc.line);

  // Update the functions upvalue state and pop the function's compile_frame
  gab_bc_frame *frame = peek_frame(self, 0);

  function->module = self->bc.mod;
  function->nupvalues = frame->upv_count;
  // Deepest local - args - caller = number of locals declared
  function->nlocals = frame->deepest_local - function->narguments - 1;

  // Patch the jump to skip over the function body
  gab_module_patch_jump(self->bc.mod, skip_jump);

  return COMP_OK;
}

i32 compile_function(gab_engine *self, s_i8 name, boolean is_lambda) {
  u64 skip_jump = gab_module_push_jump(self->bc.mod, OP_JUMP,
                                       self->bc.previous_token, self->bc.line);

  // Doesnt need a pop scope because the scope is popped with the callframe.
  down_scope(self);

  gab_obj_function *function =
      gab_obj_function_create(self->bc.mod->engine, name);

  down_frame(self, name);

  gab_token closing = is_lambda ? TOKEN_PIPE : TOKEN_RPAREN;

  if (!match_token(self, closing)) {
    i32 narguments = compile_function_args(self);

    if (narguments < 0)
      return COMP_ERR;

    function->narguments = narguments;
  }

  if (expect_token(self, closing) < 0)
    return COMP_ERR;

  if (is_lambda && expect_token(self, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  if (compile_function_body(self, function, skip_jump) < 0)
    return COMP_ERR;

  // Push a closure wrapping the function
  push_op(self, OP_CLOSURE);
  push_short(self, add_constant(self, GAB_VAL_OBJ(function)));

  gab_bc_frame *frame = peek_frame(self, 0);
  for (int i = 0; i < function->nupvalues; i++) {
    push_byte(self, frame->upvs_flag[i]);
    push_byte(self, frame->upvs_index[i]);
  }

  function->nlocals = peek_frame(self, 0)->deepest_local - function->narguments;

  up_frame(self);

  return COMP_OK;
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
i32 compile_expressions(gab_engine *self, u8 want, boolean *vse_out) {

  u8 have = 0;
  boolean vse;

  i32 result;
  do {
    result = compile_exp_prec(self, PREC_ASSIGNMENT);

    if (result < 0)
      return COMP_ERR;

    vse = result == VAR_RET;
    have++;
  } while ((result = match_and_eat_token(self, TOKEN_COMMA)));

  if (result < 0)
    return COMP_ERR;

  if (have > want) {
    write_error(self, GAB_ERROR_TOO_MANY_EXPRESSIONS, "");
    return COMP_ERR;
  }

  if (vse) {
    // If our expression list is variable length
    have--; // Don't count the variable expression itself.

    // If we want a variable number or more than we have
    if (want == VAR_RET || want > have) {
      // If the expression is variable because it ends
      // with a function call, try to patch that call to want
      // either variable or the additional number of exps.
      gab_module_try_patch_vse(self->bc.mod, want - have);
    }
  } else {
    // If our expression list is constant length
    if (want != VAR_RET) {
      // If we want a constant number of expressions
      while (have < want) {
        // While we have fewer expressions than we want, push nulls.
        push_op(self, OP_PUSH_NULL);
        have++;
      }
    }
  }

  // If we have an out argument, set its values.
  if (vse_out != NULL) {
    *vse_out = vse;
  }

  return have;
}

i32 compile_expression(gab_engine *self) {
  return compile_expressions(self, 1, NULL);
}

i32 add_string_constant(gab_engine *self, s_i8 str) {
  gab_obj_string *obj = gab_obj_string_create(self->bc.mod->engine, str);

  return add_constant(self, GAB_VAL_OBJ(obj));
}

i32 compile_id_constant(gab_engine *self) {
  if (expect_token(self, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  return add_string_constant(self, self->bc.lex.previous_token_src);
}

i32 compile_property(gab_engine *self, boolean assignable) {
  i32 prop = compile_id_constant(self);
  if (prop < 0)
    return COMP_ERR;

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE, "");
      return COMP_ERR;
    }

    if (compile_expression(self) < 0)
      return COMP_ERR;

    push_op(self, OP_SET_PROPERTY);

    push_short(self, prop);
    // The shape at this get
    gab_module_push_inline_cache(self->bc.mod, self->bc.previous_token,
                                 self->bc.line);
    // The cache'd offset
    push_short(self, 0);

    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_op(self, OP_GET_PROPERTY);
    push_short(self, prop);
    // The shape at this get
    gab_module_push_inline_cache(self->bc.mod, self->bc.previous_token,
                                 self->bc.line);
    // The cache'd offset
    push_short(self, 0);
    break;
  }

  default:
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling property access");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_lst_internal_item(gab_engine *self, u8 index) {

  if (compile_expression(self) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_lst_internals(gab_engine *self) {
  u8 size = 0;

  if (skip_newlines(self) < 0)
    return COMP_ERR;

  while (!match_token(self, TOKEN_RBRACE)) {

    if (compile_lst_internal_item(self, size) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      write_error(self, GAB_ERROR_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(self) < 0)
      return COMP_ERR;
    size++;
  }

  if (expect_token(self, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  return size;
}

// Forward decl
i32 compile_definition(gab_engine *self, s_i8 name);

i32 compile_obj_internal_item(gab_engine *self) {
  if (match_and_eat_token(self, TOKEN_IDENTIFIER)) {
    // Get the string for the key and push the key.
    s_i8 name = self->bc.lex.previous_token_src;

    gab_obj_string *obj = gab_obj_string_create(
        self->bc.mod->engine, self->bc.lex.previous_token_src);

    // Push the constant onto the stack.
    push_op(self, OP_CONSTANT);
    push_short(self, add_constant(self, GAB_VAL_OBJ(obj)));

    // Compile the expression if theres a colon, or look for a local with
    // the name and use that as the value.
    switch (match_and_eat_token(self, TOKEN_COLON)) {

    case COMP_OK: {
      if (compile_expression(self) < 0)
        return COMP_ERR;

      return COMP_OK;
    }

    case COMP_TOKEN_NO_MATCH: {
      u8 value_in;

      i32 result = resolve_id(self, name, &value_in);

      switch (result) {

      case COMP_RESOLVED_TO_LOCAL:
        gab_module_push_load_local(self->bc.mod, value_in,
                                   self->bc.previous_token, self->bc.line);
        break;

      case COMP_RESOLVED_TO_UPVALUE:
        gab_module_push_load_upvalue(
            self->bc.mod, value_in, self->bc.previous_token, self->bc.line,
            peek_frame(self, 0)->upvs_flag[value_in] & FLAG_MUTABLE);
        break;

      case COMP_ID_NOT_FOUND:
        push_op(self, OP_PUSH_TRUE);
        break;

      default:
        write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                    "While compiling object literal");
        return COMP_ERR;
      }

      return COMP_OK;
    }

    default:
      goto fin;
    }
  }

  if (match_and_eat_token(self, TOKEN_LBRACE)) {

    if (compile_expression(self) < 0)
      return COMP_ERR;

    if (expect_token(self, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    if (expect_token(self, TOKEN_COLON) < 0)
      return COMP_ERR;

    if (compile_expression(self) < 0)
      return COMP_ERR;

    return COMP_OK;
  }

  if (match_and_eat_token(self, TOKEN_DEF)) {
    i32 prop = compile_id_constant(self);

    if (prop < 0)
      return COMP_ERR;

    s_i8 name = self->bc.lex.previous_token_src;

    push_op(self, OP_CONSTANT);

    push_short(self, prop);

    if (compile_definition(self, name) < 0)
      return COMP_ERR;

    return COMP_OK;
  }

fin:
  eat_token(self);
  write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
              "While compiling object literal");
  return COMP_ERR;
}

i32 compile_obj_internals(gab_engine *self) {
  u8 size = 0;

  if (skip_newlines(self) < 0)
    return COMP_ERR;

  while (match_and_eat_token(self, TOKEN_RBRACK) == COMP_TOKEN_NO_MATCH) {

    if (compile_obj_internal_item(self) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      write_error(self, GAB_ERROR_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(self) < 0)
      return COMP_ERR;

    size++;
  };

  return size;
}

i32 compile_record(gab_engine *self, s_i8 name) {
  boolean is_def = name.data != NULL;
  i32 name_const;

  if (is_def) {
    name_const = add_string_constant(self, name);
  }

  i32 size = compile_obj_internals(self);

  if (size < 0)
    return COMP_ERR;

  push_op(self, is_def ? OP_OBJECT_RECORD_DEF : OP_OBJECT_RECORD);

  if (is_def)
    push_short(self, name_const);

  push_byte(self, size);
  return COMP_OK;
}

i32 compile_array(gab_engine *self) {
  i32 size = compile_lst_internals(self);

  if (size < 0)
    return COMP_ERR;

  push_op(self, OP_OBJECT_ARRAY);
  push_byte(self, size);
  return COMP_OK;
}

i32 compile_definition(gab_engine *self, s_i8 name) {

  if (match_and_eat_token(self, TOKEN_LPAREN)) {
    return compile_function(self, name, false);
  }

  if (match_and_eat_token(self, TOKEN_LBRACK)) {
    return compile_record(self, name);
  }

  if (match_and_eat_token(self, TOKEN_LBRACE)) {
    return compile_array(self);
  }

  write_error(self, GAB_ERROR_UNEXPECTED_TOKEN, "While compiling definition");
  return COMP_ERR;
}

//---------------- Compiling Expressions ------------------

i32 compile_exp_blk(gab_engine *self, boolean assignable) {

  down_scope(self);

  if (expect_token(self, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  i32 result = compile_block_body(self);

  if (result < 0)
    return COMP_ERR;

  up_scope(self);

  if (match_token(self, TOKEN_EOF)) {
    write_error(self, GAB_ERROR_MISSING_END, "");
    return COMP_ERR;
  }

  if (expect_token(self, TOKEN_END) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_if(gab_engine *self, boolean assignable) {

  down_scope(self);

  if (expect_token(self, TOKEN_LPAREN) < 0)
    return COMP_ERR;

  if (compile_expression(self) < 0)
    return COMP_ERR;
    
  if (expect_token(self, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  u64 then_jump = gab_module_push_jump(self->bc.mod, OP_POP_JUMP_IF_FALSE,
                                       self->bc.previous_token, self->bc.line);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  up_scope(self);

  u64 else_jump = gab_module_push_jump(self->bc.mod, OP_JUMP,
                                       self->bc.previous_token, self->bc.line);

  gab_module_patch_jump(self->bc.mod, then_jump);

  switch (match_and_eat_token(self, TOKEN_ELSE)) {
  case COMP_OK:
    if (optional_newline(self) < 0)
      return COMP_ERR;

    if (compile_expression(self) < 0)
      return COMP_ERR;

    break;

  case COMP_TOKEN_NO_MATCH:
    push_op(self, OP_PUSH_NULL);
    break;

  default:
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling if expression");
    return COMP_ERR;
  }

  gab_module_patch_jump(self->bc.mod, else_jump);

  return COMP_OK;
}

i32 compile_exp_mch(gab_engine *self, boolean assignable) {
  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  u64 next = 0;

  v_u64 done_jumps;
  v_u64_create(&done_jumps, 8);

  while (match_and_eat_token(self, TOKEN_QUESTION) == COMP_TOKEN_NO_MATCH) {
    if (next != 0)
      gab_module_patch_jump(self->bc.mod, next);

    if (compile_expression(self) < 0)
      return COMP_ERR;

    push_op(self, OP_MATCH);

    next = gab_module_push_jump(self->bc.mod, OP_POP_JUMP_IF_FALSE,
                                self->bc.previous_token, self->bc.line);

    if (expect_token(self, TOKEN_FAT_ARROW) < 0)
      return COMP_ERR;

    if (compile_expression(self) < 0)
      return COMP_ERR;

    // Push a jump out of the match statement at the end of every case.
    v_u64_push(&done_jumps,
               gab_module_push_jump(self->bc.mod, OP_JUMP,
                                    self->bc.previous_token, self->bc.line));

    if (expect_token(self, TOKEN_NEWLINE) < 0)
      return COMP_ERR;
  }

  // If none of the cases match, the last jump should end up here.
  gab_module_patch_jump(self->bc.mod, next);

  // Pop the pattern that we never matched
  push_op(self, OP_POP);

  if (expect_token(self, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expression(self) < 0)
    return COMP_ERR;

  for (i32 i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    gab_module_patch_jump(self->bc.mod, v_u64_val_at(&done_jumps, i));
  }

  v_u64_destroy(&done_jumps);

  return COMP_OK;
}

/*
 * Postfix assert expression.
 */
i32 compile_exp_asrt(gab_engine *self, boolean assignable) {
  push_op(self, OP_ASSERT);
  return COMP_OK;
}

/*
 * Postfix type expression.
 */
i32 compile_exp_type(gab_engine *self, boolean assignable) {
  push_op(self, OP_TYPE);
  return COMP_OK;
}

/*
 * Infix is expression.
 */
i32 compile_exp_is(gab_engine *self, boolean assignable) {
  push_op(self, OP_TYPE);

  if (compile_exp_prec(self, PREC_EQUALITY) < 0)
    return COMP_ERR;

  push_op(self, OP_EQUAL);

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
i32 compile_exp_bin(gab_engine *self, boolean assignable) {
  gab_token op = self->bc.previous_token;

  i32 result = compile_exp_prec(self, get_rule(op).prec + 1);

  if (result < 0)
    return COMP_ERR;

  switch (op) {
  case TOKEN_MINUS: {
    push_op(self, OP_SUBTRACT);
    break;
  }
  case TOKEN_PLUS: {
    push_op(self, OP_ADD);
    break;
  }
  case TOKEN_STAR: {
    push_op(self, OP_MULTIPLY);
    break;
  }
  case TOKEN_SLASH: {
    push_op(self, OP_DIVIDE);
    break;
  }
  case TOKEN_PERCENT: {
    push_op(self, OP_MODULO);
    break;
  }
  case TOKEN_DOT_DOT: {
    push_op(self, OP_CONCAT);
    break;
  }
  case TOKEN_EQUAL_EQUAL: {
    push_op(self, OP_EQUAL);
    break;
  }
  case TOKEN_LESSER: {
    push_op(self, OP_LESSER);
    break;
  }
  case TOKEN_LESSER_EQUAL: {
    push_op(self, OP_LESSER_EQUAL);
    break;
  }
  case TOKEN_GREATER: {
    push_op(self, OP_GREATER);
    break;
  }
  case TOKEN_GREATER_EQUAL: {
    push_op(self, OP_GREATER_EQUAL);
    break;
  }
  default: {
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling binary expression");
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
i32 compile_exp_una(gab_engine *self, boolean assignable) {
  gab_token op = self->bc.previous_token;

  i32 result = compile_exp_prec(self, PREC_UNARY);

  if (result < 0)
    return COMP_ERR;

  switch (op) {
  case TOKEN_MINUS: {
    push_op(self, OP_NEGATE);
    break;
  }
  case TOKEN_NOT: {
    push_op(self, OP_NOT);
    break;
  }
  default: {
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling unary expression");
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Returns null if an error occured.
 */
a_i8 *parse_raw_str(gab_engine *self, s_i8 raw_str) {
  // The parsed string will be at most as long as the raw string.
  // (\n -> one char)
  i8 buffer[raw_str.len];
  u64 buf_end = 0;

  // Skip the first and last character (")
  for (u64 i = 1; i < raw_str.len - 1; i++) {
    i8 c = raw_str.data[i];

    if (c == '\\') {

      i8 code;
      switch (raw_str.data[i + 1]) {

      case 'n':
        code = '\n';
        break;
      case 't':
        code = '\t';
        break;
      case '{':
        code = '{';
        break;
      case '\\':
        code = '\\';
        break;
      default:
        buffer[buf_end++] = c;
        continue;
      }

      buffer[buf_end++] = code;
      i++;

    } else {
      buffer[buf_end++] = c;
    }
  }

  return a_i8_create(buffer, buf_end);
};

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_str(gab_engine *self, boolean assignable) {

  s_i8 raw_token = self->bc.lex.previous_token_src;

  a_i8 *parsed = parse_raw_str(self, raw_token);

  if (parsed == NULL) {
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling string literal");
    return COMP_ERR;
  }

  gab_obj_string *obj = gab_obj_string_create(
      self->bc.mod->engine, s_i8_create(parsed->data, parsed->len));

  a_i8_destroy(parsed);

  push_op(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_OBJ(obj)));

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_itp(gab_engine *self, boolean assignable) {

  if (compile_exp_str(self, assignable) < 0)
    return COMP_ERR;

  if (match_token(self, TOKEN_INTERPOLATION_END)) {
    // Empty interpolation
    goto fin;
  }

  if (!match_token(self, TOKEN_INTERPOLATION)) {
    if (compile_expression(self) < 0)
      return COMP_ERR;

    push_op(self, OP_STRINGIFY);
    push_op(self, OP_CONCAT);
  }

  i32 result;
  while ((result = match_and_eat_token(self, TOKEN_INTERPOLATION))) {

    if (compile_exp_str(self, assignable) < 0)
      return COMP_ERR;

    if (match_token(self, TOKEN_INTERPOLATION_END)) {
      goto fin;
    }

    if (!match_token(self, TOKEN_INTERPOLATION)) {

      if (compile_expression(self) < 0)
        return COMP_ERR;

      push_op(self, OP_STRINGIFY);
      push_op(self, OP_CONCAT);
    }

    // concat this to the long-running string.
    push_op(self, OP_CONCAT);
  }

  if (result < 0)
    return COMP_ERR;

fin:
  if (expect_token(self, TOKEN_INTERPOLATION_END) < 0)
    return COMP_ERR;

  if (compile_exp_str(self, assignable) < 0)
    return COMP_ERR;

  // Concat the final string.
  push_op(self, OP_CONCAT);
  return COMP_OK;
}

i32 compile_exp_grp(gab_engine *self, boolean assignable) {

  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (expect_token(self, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_num(gab_engine *self, boolean assignable) {
  f64 num = strtod((char *)self->bc.lex.previous_token_src.data, NULL);
  push_op(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_NUMBER(num)));

  return COMP_OK;
}

i32 compile_exp_bool(gab_engine *self, boolean assignable) {
  push_byte(self, self->bc.previous_token == TOKEN_TRUE ? OP_PUSH_TRUE
                                                        : OP_PUSH_FALSE);
  return COMP_OK;
}

i32 compile_exp_null(gab_engine *self, boolean assignable) {
  push_op(self, OP_PUSH_NULL);
  return COMP_OK;
}

i32 compile_exp_lmd(gab_engine *self, boolean assignable) {
  s_i8 name = {0};

  if (compile_function(self, name, TOKEN_PIPE) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_def(gab_engine *self, boolean assignable) {

  if (expect_token(self, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  s_i8 name = self->bc.lex.previous_token_src;

  u8 local = add_local(self, name, 0);

  if (compile_definition(self, name) < 0)
    return COMP_ERR;

  initialize_local(self, local);

  gab_module_push_store_local(self->bc.mod, local, self->bc.previous_token,
                              self->bc.line);

  return COMP_OK;
}

i32 compile_exp_arr(gab_engine *self, boolean assignable) {
  return compile_array(self);
}

i32 compile_exp_spd(gab_engine *self, boolean assignable) {
  if (compile_expression(self) < 0)
    return COMP_ERR;

  push_op(self, OP_SPREAD);
  // Want byte
  push_byte(self, 1);

  return VAR_RET;
}

i32 compile_exp_rec(gab_engine *self, boolean assignable) {
  return compile_record(self, (s_i8){0});
}

i32 compile_exp_let(gab_engine *self, boolean assignable) {

  u8 locals[16] = {0};

  u8 local_count = 0;

  i32 result;
  do {
    if (local_count == 16) {
      write_error(self, GAB_ERROR_TOO_MANY_EXPRESSIONS_IN_LET, "");
      return COMP_ERR;
    }

    if (expect_token(self, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = self->bc.lex.previous_token_src;

    i32 result = resolve_id(self, self->bc.lex.previous_token_src, NULL);

    switch (result) {

    case COMP_ID_NOT_FOUND: {
      i32 loc = compile_local(self, name, FLAG_MUTABLE);

      if (loc < 0)
        return COMP_ERR;

      locals[local_count] = loc;
      break;
    }

    case COMP_RESOLVED_TO_LOCAL:
    case COMP_RESOLVED_TO_UPVALUE: {
      write_error(self, GAB_ERROR_LOCAL_ALREADY_EXISTS, "");
      break;
    }

    default:
      write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                  "While compiling let expression");
      return COMP_ERR;
    }

    local_count++;

  } while ((result = match_and_eat_token(self, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (compile_expressions(self, local_count, NULL) < 0)
      return COMP_ERR;
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    write_error(self, GAB_ERROR_MISSING_INITIALIZER, "");
    return COMP_ERR;

  default:
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling 'let' expression");
    return COMP_ERR;
  }

  while (local_count--) {
    u8 local = locals[local_count];
    initialize_local(self, local);

    if (local_count > 0) {
      push_op(self, OP_POP_STORE_LOCAL);
      push_byte(self, local);
    } else {
      gab_module_push_store_local(self->bc.mod, local, self->bc.previous_token,
                                  self->bc.line);
    }
  }

  return COMP_OK;
}

i32 compile_exp_idn(gab_engine *self, boolean assignable) {
  s_i8 name = self->bc.lex.previous_token_src;

  u8 var;
  boolean is_local_var = true;

  switch (resolve_id(self, name, &var)) {
  case COMP_RESOLVED_TO_LOCAL: {
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    is_local_var = false;
    break;
  }

  case COMP_ID_NOT_FOUND: {
    write_error(self, GAB_ERROR_MISSING_IDENTIFIER, "");
    return COMP_ERR;
  }

  default:
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN, "While compiling identifier");
    return COMP_ERR;
  }

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE,
                  "While compiling identifier");
      return COMP_ERR;
    }

    gab_bc_frame *frame = peek_frame(self, 0);
    if (is_local_var) {
      if (!(frame->locals_flag[var] & FLAG_MUTABLE)) {
        write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE,
                    "Variable is not mutable");
      }
    } else {
      if (!(frame->upvs_flag[var] & FLAG_MUTABLE)) {
        write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE,
                    "Variable is not mutable");
      }
    }

    if (compile_expression(self) < 0)
      return COMP_ERR;

    if (is_local_var) {
      gab_module_push_store_local(self->bc.mod, var, self->bc.previous_token,
                                  self->bc.line);
    } else {
      gab_module_push_store_upvalue(self->bc.mod, var, self->bc.previous_token,
                                    self->bc.line);
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    if (is_local_var) {
      gab_module_push_load_local(self->bc.mod, var, self->bc.previous_token,
                                 self->bc.line);
    } else {
      gab_module_push_load_upvalue(
          self->bc.mod, var, self->bc.previous_token, self->bc.line,
          peek_frame(self, 0)->upvs_flag[var] & FLAG_MUTABLE);
    }
    break;

  default:
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN,
                "While compiling 'identifier' expression");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_idx(gab_engine *self, boolean assignable) {
  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (expect_token(self, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (assignable) {
      if (compile_expression(self) < 0)
        return COMP_ERR;

      push_op(self, OP_SET_INDEX);
    } else {
      write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE,
                  "While compiling 'index' expression");
      return COMP_ERR;
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_op(self, OP_GET_INDEX);
    break;
  }

  default:
    write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE,
                "While compiling 'index' expression");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_rec_call(gab_engine *self, boolean assignable) {
  if (compile_exp_rec(self, assignable) < 0)
    return COMP_ERR;

  gab_module_push_call(self->bc.mod, 1, false, self->bc.previous_token,
                       self->bc.line);

  return VAR_RET;
}

i32 compile_exp_call(gab_engine *self, boolean assignable) {
  boolean vse = false;
  i32 result = 0;

  if (!match_token(self, TOKEN_RPAREN)) {
    result = compile_expressions(self, VAR_RET, &vse);

    if (result < 0)
      return COMP_ERR;
  }

  if (!vse && result > 16) {
    write_error(self, GAB_ERROR_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_call(self->bc.mod, result, vse, self->bc.previous_token,
                       self->bc.line);

  if (expect_token(self, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return VAR_RET;
}

i32 compile_exp_dot(gab_engine *self, boolean assignable) {
  if (compile_property(self, assignable) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_glb(gab_engine *self, boolean assignable) {
  u8 global;

  switch (resolve_id(self, s_i8_create((i8 *)"__global__", 10), &global)) {
  case COMP_RESOLVED_TO_LOCAL: {
    gab_module_push_load_local(self->bc.mod, global, self->bc.previous_token,
                               self->bc.line);
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    gab_module_push_load_upvalue(self->bc.mod, global, self->bc.previous_token,
                                 self->bc.line, false);
    break;
  }

  default:
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN, "While compiling global");
    return COMP_ERR;
  }

  if (compile_property(self, assignable) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_mth(gab_engine *self, boolean assignable) {
  // Compile the function that is being called on the top of the stack.
  if (compile_exp_prec(self, PREC_PROPERTY) < 0)
    return COMP_ERR;

  /* Stack has now
    | method    |
    | receiver  |
  */

  // Swap the method and receiver
  push_op(self, OP_SWAP);

  boolean vse = false;
  i32 result = 0;

  if (expect_token(self, TOKEN_LPAREN) < 0)
    return COMP_ERR;

  if (!match_token(self, TOKEN_RPAREN)) {

    result = compile_expressions(self, VAR_RET, &vse);

    if (result < 0)
      return COMP_ERR;
  }

  if (expect_token(self, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  /*
    | args..    |
    | receiver  |
    | method    |

   Stack has now
  */

  // Count the self argument
  result++;

  if (result > 16) {
    write_error(self, GAB_ERROR_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_call(self->bc.mod, result, vse, self->bc.previous_token,
                       self->bc.line);

  return VAR_RET;
}

i32 compile_exp_and(gab_engine *self, boolean assignable) {
  u64 end_jump = gab_module_push_jump(self->bc.mod, OP_LOGICAL_AND,
                                      self->bc.previous_token, self->bc.line);

  if (compile_exp_prec(self, PREC_AND) < 0)
    return COMP_ERR;

  gab_module_patch_jump(self->bc.mod, end_jump);

  return COMP_OK;
}

i32 compile_exp_or(gab_engine *self, boolean assignable) {
  u64 end_jump = gab_module_push_jump(self->bc.mod, OP_LOGICAL_OR,
                                      self->bc.previous_token, self->bc.line);

  if (compile_exp_prec(self, PREC_OR) < 0)
    return COMP_ERR;

  gab_module_patch_jump(self->bc.mod, end_jump);

  return COMP_OK;
}

i32 compile_exp_prec(gab_engine *self, gab_precedence prec) {

  if (eat_token(self) < 0)
    return COMP_ERR;

  gab_compile_rule rule = get_rule(self->bc.previous_token);

  if (rule.prefix == NULL) {
    write_error(self, GAB_ERROR_UNEXPECTED_TOKEN, "Expected an expression");
    return COMP_ERR;
  }

  boolean assignable = prec <= PREC_ASSIGNMENT;

  i32 have = rule.prefix(self, assignable);
  if (have < 0)
    return COMP_ERR;

  while (prec <= get_rule(self->bc.current_token).prec) {

    if (have < 0)
      return COMP_ERR;

    if (eat_token(self) < 0)
      return COMP_ERR;

    rule = get_rule(self->bc.previous_token);

    // Maybe try and see if the current token has a rule for prefix here?
    if (rule.infix == NULL) {
      // No infix rule for this token it might be a postfix though.

      if (rule.postfix == NULL) {
        write_error(self, GAB_ERROR_UNEXPECTED_TOKEN, "");
        return COMP_ERR;
      }

      have = rule.postfix(self, assignable);
    } else {
      // Treat this as an infix expression.
      have = rule.infix(self, assignable);
    }
  }

  // TODO: See if this actually does anything.
  if (assignable && match_token(self, TOKEN_EQUAL)) {
    write_error(self, GAB_ERROR_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  return have;
}

i32 compile_exp_for(gab_engine *self, boolean assignable) {
  down_scope(self);

  i32 iter = add_invisible_local(self);
  if (iter < 0)
    return COMP_ERR;

  u16 loop_locals = 0;
  i32 result;

  do {

    if (expect_token(self, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = self->bc.lex.previous_token_src;

    i32 loc = compile_local(self, name, 0);

    if (loc < 0)
      return COMP_ERR;

    initialize_local(self, loc);
    loop_locals++;
  } while ((result = match_and_eat_token(self, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

  if (expect_token(self, TOKEN_IN) < 0)
    return COMP_ERR;

  // This is the iterator function
  if (compile_expression(self) < 0)
    return COMP_ERR;

  // Store the iterator into the iter local.
  push_op(self, OP_POP_STORE_LOCAL);
  push_byte(self, iter);

  u64 loop_start = self->bc.mod->bytecode.len - 1;

  // Load the funciton.
  gab_module_push_load_local(self->bc.mod, iter, self->bc.previous_token,
                             self->bc.line);

  // Call the function, wanting loop_locals results.
  push_op(self, OP_CALL_0);
  push_byte(self, loop_locals);

  // Pop the results in reverse order, assigning them to each loop local.
  for (u8 ll = 0; ll < loop_locals; ll++) {
    push_op(self, OP_POP_STORE_LOCAL);
    push_byte(self, iter + loop_locals - ll);
  }

  gab_module_push_load_local(self->bc.mod, iter + loop_locals,
                             self->bc.previous_token, self->bc.line);

  // Exit the for loop if the last loop local is false.
  u64 jump_start = gab_module_push_jump(self->bc.mod, OP_JUMP_IF_FALSE,
                                        self->bc.previous_token, self->bc.line);

  if (optional_newline(self) < 0)
    return COMP_ERR;

  push_op(self, OP_POP);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  push_op(self, OP_POP);

  gab_module_push_loop(self->bc.mod, loop_start, self->bc.previous_token,
                       self->bc.line);

  gab_module_patch_jump(self->bc.mod, jump_start);

  up_scope(self);

  return COMP_OK;
}

i32 compile_exp_whl(gab_engine *self, boolean assignable) {
  u64 loop_start = self->bc.mod->bytecode.len - 1;

  down_scope(self);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  u64 jump = gab_module_push_jump(self->bc.mod, OP_POP_JUMP_IF_FALSE,
                                  self->bc.previous_token, self->bc.line);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  up_scope(self);

  gab_module_push_loop(self->bc.mod, loop_start, self->bc.previous_token,
                       self->bc.line);

  gab_module_patch_jump(self->bc.mod, jump);

  push_op(self, OP_PUSH_NULL);

  return COMP_OK;
}

i32 compile_exp_rtn(gab_engine *self, boolean assignable) {
  if (match_token(self, TOKEN_NEWLINE)) {
    push_op(self, OP_PUSH_NULL);
    push_op(self, OP_RETURN_1);
    return COMP_OK;
  }

  boolean vse;
  i32 result = compile_expressions(self, VAR_RET, &vse);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    write_error(self, GAB_ERROR_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_module_push_return(self->bc.mod, result, vse, self->bc.previous_token,
                         self->bc.line);
  return COMP_OK;
}

// All of the expression compiling functions follow the naming convention
// compile_exp_XXX.
#define NONE()                                                                 \
  { NULL, NULL, NULL, PREC_NONE, false }
#define PREFIX(fnc)                                                            \
  { compile_exp_##fnc, NULL, NULL, PREC_NONE, false }
#define INFIX(fnc, prec, is_multi)                                             \
  { NULL, compile_exp_##fnc, NULL, PREC_##prec, is_multi }
#define PREFIX_INFIX(pre, in, prec, is_multi)                                  \
  { compile_exp_##pre, compile_exp_##in, NULL, PREC_##prec, is_multi }
#define POSTFIX(fnc)                                                           \
  { NULL, NULL, compile_exp_##fnc, PREC_PROPERTY, false }
#define PREFIX_POSTFIX(pre, post)                                              \
  { compile_exp_##pre, NULL, compile_exp_##post, PREC_PROPERTY, false }

// ----------------Pratt Parsing Table ----------------------
const gab_compile_rule gab_bc_rules[] = {
    PREFIX(let),                       // LET
    PREFIX(mch),                       // MATCH
    PREFIX(if),                        // IF
    NONE(),                            // ELSE
    PREFIX(blk),                       // DO
    PREFIX(for),                       // FOR
    NONE(),                            // IN
    INFIX(is, COMPARISON, false),                            // IS
    NONE(),                            // END
    PREFIX(def),                       // DEF
    PREFIX(rtn),                       // RETURN
    PREFIX(whl),                       // WHILE
    INFIX(bin, TERM, false),                  // PLUS
    PREFIX_INFIX(una, bin, TERM, false),      // MINUS
    INFIX(bin, FACTOR, false),                // STAR
    INFIX(bin, FACTOR, false),                // SLASH
    INFIX(bin, FACTOR, false),                // PERCENT
    NONE(),                            // COMMA
    INFIX(mth, CALL, true),       // COLON
    NONE(),           // AMPERSAND
    INFIX(dot, PROPERTY, true),              // DOT
    PREFIX_INFIX(spd, bin, TERM, false),                  // DOT_DOT
    NONE(),                            // EQUAL
    INFIX(bin, EQUALITY, false),              // EQUALEQUAL
    POSTFIX(type),                            // QUESTION
    PREFIX_POSTFIX(glb, asrt),                      // BANG
    NONE(),                            // AT
    NONE(),                            // COLON_EQUAL
    INFIX(bin, COMPARISON, false),            // LESSER
    INFIX(bin, EQUALITY, false),              // LESSEREQUAL
    INFIX(bin, COMPARISON, false),            // GREATER
    INFIX(bin, EQUALITY, false),              // GREATEREQUAL
    NONE(),                            // ARROW
    NONE(),                            // FATARROW
    INFIX(and, AND, false),                   // AND
    INFIX(or, OR, false),                     // OR
    PREFIX(una),                       // NOT
    PREFIX_INFIX(arr, idx, PROPERTY, false),  // LBRACE
    NONE(),                            // RBRACE
    PREFIX_INFIX(rec, rec_call, CALL, false), // LBRACK
    NONE(),                            // RBRACK
    PREFIX_INFIX(grp, call, CALL, false),     // LPAREN
    NONE(),                            // RPAREN
    PREFIX(lmd),            // PIPE
    NONE(),                            // SEMI
    PREFIX(idn),                       // ID
    PREFIX(str),                       // STRING
    PREFIX(itp),                       // INTERPOLATION
    NONE(),                       // INTERPOLATION END
    PREFIX(num),                       // NUMBER
    PREFIX(bool),                      // FALSE
    PREFIX(bool),                      // TRUE
    PREFIX(null),                      // NULL
    NONE(),                      // NEWLINE
    NONE(),                            // EOF
    NONE(),                            // ERROR
};

gab_compile_rule get_rule(gab_token k) { return gab_bc_rules[k]; }

gab_obj_closure *compile(gab_engine *self, gab_module *mod, s_i8 name) {
  // Reset the bytecode compiler.
  gab_bc_create(&self->bc);

  self->bc.mod = mod;

  down_frame(self, name);

  gab_lexer_create(&self->bc.lex, self->bc.mod->source);

  self->bc.mod->source_lines = self->bc.lex.source_lines;

  if (eat_token(self) == COMP_ERR)
    return NULL;

  initialize_local(self,
                   add_local(self, s_i8_create((i8 *)"__global__", 10), 0));

  if (compile_block_body(self) == COMP_ERR)
    return NULL;

  gab_obj_function *main_func =
      gab_obj_function_create(self->bc.mod->engine, name);

  gab_add_constant(self->bc.mod->engine, GAB_VAL_OBJ(main_func));

  main_func->module = self->bc.mod;
  main_func->narguments = 1;
  main_func->nlocals = peek_frame(self, 0)->deepest_local;

  gab_obj_closure *main =
      gab_obj_closure_create(self->bc.mod->engine, main_func, NULL);

  up_frame(self);

  push_op(self, OP_RETURN_1);

  return main;
  ;
}

static u64 write_safe(a_i8 *dest, u64 offset, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  const u64 bytes_written =
      vsnprintf((char *)dest->data + offset, dest->len - offset, fmt, args);

  va_end(args);

  return bytes_written;
}

gab_result gab_bc_compile(gab_engine *eng, s_i8 name, s_i8 source, u8 flags) {

  gab_module *module = gab_add_module(eng, name, source);

  gab_obj_closure *main = compile(eng, module, name);

  if (main == NULL) {
    return (gab_result){.k = GAB_RESULT_COMPILE_ERROR};
  }

  if (flags & GAB_FLAG_DUMP_BYTECODE) {
    gab_module_dump(module, name);
  }

  gab_value result = GAB_VAL_OBJ(main);

  return (gab_result){.k = GAB_RESULT_OK, .result = result};
}

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

static void write_error(gab_engine *gab, gab_error_k e, const char *help_fmt,
                        ...) {
  if (gab->bc.panic)
    return;

  gab_lexer_finish_line(&gab->bc.lex);
  gab->bc.panic = true;

  gab_bc *compiler = &gab->bc;
  u64 bytes_written = 0;

  gab_bc_frame *frame = &compiler->frames[compiler->frame_count - 1];
  s_i8 curr_token = compiler->lex.previous_token_src;
  u64 curr_src_index = compiler->line - 1;
  s_i8 curr_src;
  if (curr_src_index < compiler->lex.source_lines->len) {
    curr_src = v_s_i8_val_at(compiler->lex.source_lines, curr_src_index);
  } else {
    curr_src = s_i8_cstr("");
  }

  s_i8 func_name = frame->locals_name[0];

  a_i8 *curr_under = a_i8_empty(curr_src.len);

  i8 *tok_start, *tok_end;

  tok_start = curr_token.data;
  tok_end = curr_token.data + curr_token.len;

  const char *tok = gab_token_names[compiler->previous_token];

  for (u8 i = 0; i < curr_under->len; i++) {
    if (curr_src.data + i >= tok_start && curr_src.data + i < tok_end)
      curr_under->data[i] = '^';
    else
      curr_under->data[i] = ' ';
  }

  const char *curr_color = ANSI_COLOR_RED;

  const char *curr_box = "\u256d";

  bytes_written += write_safe(
      gab->error, 0,
      "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET "] line " ANSI_COLOR_RED
      "%04lu" ANSI_COLOR_RESET ": Error near " ANSI_COLOR_BLUE
      "%s" ANSI_COLOR_RESET "\n\t%s%s %.4lu " ANSI_COLOR_RESET "%.*s"
      "\n\t\u2502      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET
      "\n\t\u2570\u2500> ",
      (i32)func_name.len, func_name.data, compiler->line, tok, curr_box,
      curr_color, curr_src_index + 1, (i32)curr_src.len, curr_src.data,
      (i32)curr_under->len, curr_under->data);

  a_i8_destroy(curr_under);

  bytes_written += write_safe(gab->error, bytes_written,
                              ANSI_COLOR_YELLOW "%s\n\t\t", gab_error_names[e]);

  va_list args;
  va_start(args, help_fmt);

  bytes_written += write_safe(gab->error, bytes_written, help_fmt, args);

  va_end(args);

  bytes_written += write_safe(gab->error, bytes_written, "\n" ANSI_COLOR_RESET);
}
