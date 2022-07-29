#include "compiler.h"
#include "engine.h"
#include "module.h"
#include "object.h"
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

typedef i32 (*gab_compile_func)(gab_compiler *, boolean);

typedef struct gab_compile_rule gab_compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  gab_compile_func postfix;
  gab_precedence prec;
  boolean multi_line;
};

void gab_compiler_create(gab_compiler *self, gab_module *mod) {
  self->scope_depth = 0;
  self->frame_count = 0;
  self->error = NULL;
  self->mod = mod;
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

static boolean match_token(gab_compiler *self, gab_token tok);

static i32 eat_token(gab_compiler *self);

void set_err(gab_compiler *self, const char *err) {
  if (self->error == NULL) {
    gab_lexer_finish_line(&self->lex);
    self->error = err;
  }
}

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline boolean match_token(gab_compiler *self, gab_token tok) {
  return self->current_token == tok;
}

// Returns less than 0 if there was an error, greater than 0 otherwise.
static i32 eat_token(gab_compiler *self) {
  self->previous_token = self->current_token;
  self->current_token = gab_lexer_next(&self->lex);
  self->line = self->lex.current_row;

  if (match_token(self, TOKEN_ERROR)) {
    eat_token(self);
    set_err(self, self->lex.error_msg);
    return COMP_ERR;
  }

  return COMP_OK;
}

static i32 match_and_eat_token(gab_compiler *self, gab_token tok) {
  if (!match_token(self, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(self);
}

static i32 expect_token(gab_compiler *self, gab_token tok) {
  if (!match_token(self, tok)) {
    eat_token(self);
    set_err(self, "Unexpected token");
    return COMP_ERR;
  }

  return eat_token(self);
}

static inline u16 add_constant(gab_compiler *self, gab_value value) {
  u16 index = gab_engine_add_constant(self->mod->engine, value);
  return index;
}

static inline void push_op(gab_compiler *self, gab_opcode op) {
  gab_module_push_op(self->mod, op, self->previous_token, self->line);
}

static inline void push_byte(gab_compiler *self, u8 data) {
  gab_module_push_byte(self->mod, data, self->previous_token, self->line);
}

static inline void push_short(gab_compiler *self, u16 data) {
  gab_module_push_short(self->mod, data, self->previous_token, self->line);
}

//--------------------- Local Variable Helpers --------------
static gab_compile_frame *peek_frame(gab_compiler *self, u32 depth) {
  return &self->frames[self->frame_count - depth - 1];
}

static void initialize_local(gab_compiler *self, u8 local) {
  peek_frame(self, 0)->locals_depth[local] = self->scope_depth;
}

static i32 add_invisible_local(gab_compiler *self) {
  gab_compile_frame *frame = peek_frame(self, 0);

  if (frame->local_count == LOCAL_MAX) {
    set_err(self, "Can't have more than 255 variables in a function");
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
static i32 add_local(gab_compiler *self, s_i8 name, u8 flags) {
  gab_compile_frame *frame = peek_frame(self, 0);
  if (frame->local_count == LOCAL_MAX) {
    set_err(self, "Can't have more than 255 variables in a function");
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
static i32 add_upvalue(gab_compiler *self, u32 depth, u8 index, u8 flags) {
  gab_compile_frame *frame = peek_frame(self, depth);
  u16 count = frame->upv_count;

  // Don't pull redundant upvalues
  for (int i = 0; i < count; i++) {
    if (frame->upvs_index[i] == index && (frame->upvs_flag[i]) == flags) {
      return i;
    }
  }

  if (count == UPVALUE_MAX) {
    set_err(self, "Can't capture more than 255 variables in one closure");
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
static i32 resolve_local(gab_compiler *self, s_i8 name, u32 depth) {
  for (i32 local = peek_frame(self, depth)->local_count - 1; local >= 0;
       local--) {
    if (s_i8_match(name, peek_frame(self, depth)->locals_name[local])) {
      if (peek_frame(self, depth)->locals_depth[local] == -1) {
        set_err(self, "Can't reference a variable before it is initialized");
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
static i32 resolve_upvalue(gab_compiler *self, s_i8 name, u32 depth) {
  // Base case, hopefully conversion doesn't cause issues
  if (depth >= self->frame_count - 1) {
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
static i32 resolve_id(gab_compiler *self, s_i8 name, u8 *value_in) {

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

static void down_scope(gab_compiler *self) { self->scope_depth++; }

static void up_scope(gab_compiler *self) {
  self->scope_depth--;

  gab_compile_frame *frame = peek_frame(self, 0);

  boolean capture = false;
  while (frame->local_count > 1 &&
         frame->locals_depth[frame->local_count - 1] > self->scope_depth) {
    capture |= frame->locals_flag[frame->local_count - 1] & FLAG_CAPTURED;
    frame->local_count--;
  }

  // If this block contained a captured local, escape it.
  if (capture) {
    gab_module_push_op(self->mod, OP_CLOSE_UPVALUE, self->previous_token,
                       self->line);
    gab_module_push_byte(self->mod, frame->local_count, self->previous_token,
                         self->line);
  }
}

static void down_frame(gab_compiler *self, s_i8 name) {
  self->frame_count++;
  // The first local in any given frame is reserved for the function being
  // called. It is immutable.
  peek_frame(self, 0)->local_count = 0;
  peek_frame(self, 0)->upv_count = 0;
  peek_frame(self, 0)->deepest_local = 0;
  peek_frame(self, 0)->function_name = name;

  i32 loc = add_local(self, name, 0);
  initialize_local(self, loc);
}

static void up_frame(gab_compiler *self) {
  peek_frame(self, 0)->local_count = 0;
  peek_frame(self, 0)->upv_count = 0;
  self->frame_count--;
  self->scope_depth--;
}

// Forward declare some functions
gab_compile_rule get_rule(gab_token k);
i32 compile_exp_prec(gab_compiler *self, gab_precedence prec);
i32 compile_expressions(gab_compiler *self, u8 want, boolean *vse_out);
i32 compile_expression(gab_compiler *self);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

/* Returns COMP_ERR if an error was encountered, and otherwise the offset of the
 * local.
 */
static i32 compile_local(gab_compiler *self, s_i8 name, u8 flags) {

  for (i32 local = peek_frame(self, 0)->local_count - 1; local >= 0; local--) {
    if (peek_frame(self, 0)->locals_depth[local] != -1 &&
        peek_frame(self, 0)->locals_depth[local] < self->scope_depth) {
      break;
    }

    if (s_i8_match(name, peek_frame(self, 0)->locals_name[local])) {
      set_err(self, "There is already a variable with this name in scope");
      return COMP_ERR;
    }
  }

  return add_local(self, name, flags);
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
i32 compile_function_args(gab_compiler *self) {

  i32 result = 0;
  u8 narguments = 0;

  do {

    if (narguments == FARG_MAX) {
      set_err(self, "Functions can't have more than 16 parameters");
      return COMP_ERR;
    }

    narguments++;

    if (expect_token(self, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = self->lex.previous_token_src;
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

static inline i32 skip_newlines(gab_compiler *self) {
  while (match_token(self, TOKEN_NEWLINE)) {
    if (eat_token(self) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 optional_newline(gab_compiler *self) {
  match_and_eat_token(self, TOKEN_NEWLINE);
  return COMP_OK;
}

i32 compile_block_expression(gab_compiler *self) {
  i32 result = compile_expression(self);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(self, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  if (skip_newlines(self) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_block_body(gab_compiler *self) {
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
i32 compile_function_body(gab_compiler *self, gab_obj_function *function,
                          u64 skip_jump) {
  // Start compiling the function's bytecode.
  // You can't just store a pointer to the beginning of the code
  // Because if the vector is EVER resized,
  // Then all your functions are suddenly invalid.
  function->offset = self->mod->bytecode.len;

  boolean vse;
  i32 result = compile_expressions(self, VAR_RET, &vse);

  if (result < 0)
    return COMP_ERR;

  gab_module_push_return(self->mod, result, vse, self->previous_token,
                         self->line);

  // Update the functions upvalue state and pop the function's compile_frame
  gab_compile_frame *frame = peek_frame(self, 0);

  function->module = self->mod;
  function->nupvalues = frame->upv_count;
  // Deepest local - args - caller = number of locals declared
  function->nlocals = frame->deepest_local - function->narguments - 1;

  // Patch the jump to skip over the function body
  gab_module_patch_jump(self->mod, skip_jump);

  return COMP_OK;
}

i32 compile_function(gab_compiler *self, s_i8 name, boolean is_lambda) {
  u64 skip_jump = gab_module_push_jump(self->mod, OP_JUMP, self->previous_token,
                                       self->line);

  // Doesnt need a pop scope because the scope is popped with the callframe.
  down_scope(self);

  gab_obj_function *function = gab_obj_function_create(self->mod->engine, name);

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

  gab_compile_frame *frame = peek_frame(self, 0);
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
i32 compile_expressions(gab_compiler *self, u8 want, boolean *vse_out) {

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
    set_err(self, "Too many expressions in list");
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
      gab_module_try_patch_vse(self->mod, want - have);
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

i32 compile_expression(gab_compiler *self) {
  return compile_expressions(self, 1, NULL);
}

i32 add_string_constant(gab_compiler *self, s_i8 str) {
  gab_obj_string *obj = gab_obj_string_create(self->mod->engine, str);

  return add_constant(self, GAB_VAL_OBJ(obj));
}

i32 compile_id_constant(gab_compiler *self) {
  if (expect_token(self, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  return add_string_constant(self, self->lex.previous_token_src);
}

i32 compile_property(gab_compiler *self, boolean assignable) {
  i32 prop = compile_id_constant(self);
  if (prop < 0)
    return COMP_ERR;

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      set_err(self, "Expression is not assignable");
      return COMP_ERR;
    }

    if (compile_expression(self) < 0)
      return COMP_ERR;

    push_op(self, OP_SET_PROPERTY);

    push_short(self, prop);
    // The shape at this get
    gab_module_push_inline_cache(self->mod, self->previous_token, self->line);
    // The cache'd offset
    push_short(self, 0);

    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_op(self, OP_GET_PROPERTY);
    push_short(self, prop);
    // The shape at this get
    gab_module_push_inline_cache(self->mod, self->previous_token, self->line);
    // The cache'd offset
    push_short(self, 0);
    break;
  }

  default:
    set_err(self, "Unexpected result compiling property access");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_lst_internal_item(gab_compiler *self, u8 index) {

  if (compile_expression(self) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_lst_internals(gab_compiler *self) {
  u8 size = 0;

  if (skip_newlines(self) < 0)
    return COMP_ERR;

  while (!match_token(self, TOKEN_RBRACE)) {

    if (compile_lst_internal_item(self, size) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      set_err(self, "Can't initialize an object with more than 255 values");
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
i32 compile_definition(gab_compiler *self, s_i8 name);

i32 compile_obj_internal_item(gab_compiler *self) {
  if (match_and_eat_token(self, TOKEN_IDENTIFIER)) {
    // Get the string for the key and push the key.
    s_i8 name = self->lex.previous_token_src;

    gab_obj_string *obj =
        gab_obj_string_create(self->mod->engine, self->lex.previous_token_src);

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
        gab_module_push_load_local(self->mod, value_in, self->previous_token,
                                   self->line);
        break;

      case COMP_RESOLVED_TO_UPVALUE:
        gab_module_push_load_upvalue(
            self->mod, value_in, self->previous_token, self->line,
            peek_frame(self, 0)->upvs_flag[value_in] & FLAG_MUTABLE);
        break;

      case COMP_ID_NOT_FOUND:
        push_op(self, OP_PUSH_TRUE);
        break;

      default:
        set_err(self, "Unexpected result");
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

    s_i8 name = self->lex.previous_token_src;

    push_op(self, OP_CONSTANT);

    push_short(self, prop);

    if (compile_definition(self, name) < 0)
      return COMP_ERR;

    return COMP_OK;
  }

fin:
  eat_token(self);
  set_err(self, "Unexpected token in object literal");
  return COMP_ERR;
}

i32 compile_obj_internals(gab_compiler *self) {
  u8 size = 0;

  if (skip_newlines(self) < 0)
    return COMP_ERR;

  while (match_and_eat_token(self, TOKEN_RBRACK) == COMP_TOKEN_NO_MATCH) {

    if (compile_obj_internal_item(self) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      set_err(self, "Can't initialize an object with more than 255 values");
      return COMP_ERR;
    }

    if (skip_newlines(self) < 0)
      return COMP_ERR;

    size++;
  };

  return size;
}

i32 compile_record(gab_compiler *self, s_i8 name) {
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

i32 compile_array(gab_compiler *self) {
  i32 size = compile_lst_internals(self);

  if (size < 0)
    return COMP_ERR;

  push_op(self, OP_OBJECT_ARRAY);
  push_byte(self, size);
  return COMP_OK;
}

i32 compile_definition(gab_compiler *self, s_i8 name) {

  if (match_and_eat_token(self, TOKEN_LPAREN)) {
    return compile_function(self, name, false);
  }

  if (match_and_eat_token(self, TOKEN_LBRACK)) {
    return compile_record(self, name);
  }

  if (match_and_eat_token(self, TOKEN_LBRACE)) {
    return compile_array(self);
  }

  set_err(self, "Unknown definition");
  return COMP_ERR;
}

//---------------- Compiling Expressions ------------------

i32 compile_exp_blk(gab_compiler *self, boolean assignable) {

  down_scope(self);

  if (expect_token(self, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  i32 result = compile_block_body(self);

  if (result < 0)
    return COMP_ERR;

  up_scope(self);

  if (match_token(self, TOKEN_EOF)) {
    set_err(self, "Open do expression");
    return COMP_ERR;
  }

  if (expect_token(self, TOKEN_END) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_if(gab_compiler *self, boolean assignable) {

  down_scope(self);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  u64 then_jump = gab_module_push_jump(self->mod, OP_POP_JUMP_IF_FALSE,
                                       self->previous_token, self->line);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  up_scope(self);

  u64 else_jump = gab_module_push_jump(self->mod, OP_JUMP, self->previous_token,
                                       self->line);

  gab_module_patch_jump(self->mod, then_jump);

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
    set_err(self, "Unexpected result compiling if expression");
    return COMP_ERR;
  }

  gab_module_patch_jump(self->mod, else_jump);

  return COMP_OK;
}

i32 compile_exp_mch(gab_compiler *self, boolean assignable) {
  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  u64 next = 0;

  v_u64 done_jumps;
  v_u64_create(&done_jumps, 8);

  while (match_and_eat_token(self, TOKEN_QUESTION) == COMP_TOKEN_NO_MATCH) {
    if (next != 0)
      gab_module_patch_jump(self->mod, next);

    if (compile_expression(self) < 0)
      return COMP_ERR;

    push_op(self, OP_MATCH);

    next = gab_module_push_jump(self->mod, OP_POP_JUMP_IF_FALSE,
                                self->previous_token, self->line);

    if (expect_token(self, TOKEN_FAT_ARROW) < 0)
      return COMP_ERR;

    if (compile_expression(self) < 0)
      return COMP_ERR;

    // Push a jump out of the match statement at the end of every case.
    v_u64_push(&done_jumps,
               gab_module_push_jump(self->mod, OP_JUMP, self->previous_token,
                                    self->line));

    if (expect_token(self, TOKEN_NEWLINE) < 0)
      return COMP_ERR;
  }

  // If none of the cases match, the last jump should end up here.
  gab_module_patch_jump(self->mod, next);

  // Pop the pattern that we never matched
  push_op(self, OP_POP);

  if (expect_token(self, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expression(self) < 0)
    return COMP_ERR;

  for (i32 i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    gab_module_patch_jump(self->mod, v_u64_val_at(&done_jumps, i));
  }

  v_u64_destroy(&done_jumps);

  return COMP_OK;
}

/*
 * Postfix assert expression.
 */
i32 compile_exp_asrt(gab_compiler *self, boolean assignable) {
  push_op(self, OP_ASSERT);
  return COMP_OK;
}

/*
 * Postfix type expression.
 */
i32 compile_exp_type(gab_compiler *self, boolean assignable) {
  push_op(self, OP_TYPE);
  return COMP_OK;
}

/*
 * Infix is expression.
 */
i32 compile_exp_is(gab_compiler *self, boolean assignable) {
  push_op(self, OP_TYPE);

  if (compile_exp_prec(self, PREC_EQUALITY) < 0)
    return COMP_ERR;

  push_op(self, OP_EQUAL);

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
i32 compile_exp_bin(gab_compiler *self, boolean assignable) {
  gab_token op = self->previous_token;

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
    set_err(self, "Unknown binary operator");
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
i32 compile_exp_una(gab_compiler *self, boolean assignable) {
  gab_token op = self->previous_token;

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
    set_err(self, "Unknown unary operator");
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Returns null if an error occured.
 */
a_i8 *parse_raw_str(gab_compiler *self, s_i8 raw_str) {
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
i32 compile_exp_str(gab_compiler *self, boolean assignable) {

  s_i8 raw_token = self->lex.previous_token_src;

  a_i8 *parsed = parse_raw_str(self, raw_token);

  if (parsed == NULL) {
    set_err(self, "String parsing failed");
    return COMP_ERR;
  }

  gab_obj_string *obj = gab_obj_string_create(
      self->mod->engine, s_i8_create(parsed->data, parsed->len));

  a_i8_destroy(parsed);

  push_op(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_OBJ(obj)));

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_itp(gab_compiler *self, boolean assignable) {

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

i32 compile_exp_grp(gab_compiler *self, boolean assignable) {

  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (expect_token(self, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_num(gab_compiler *self, boolean assignable) {
  f64 num = strtod((char *)self->lex.previous_token_src.data, NULL);
  push_op(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_NUMBER(num)));

  return COMP_OK;
}

i32 compile_exp_bool(gab_compiler *self, boolean assignable) {
  push_byte(self,
            self->previous_token == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  return COMP_OK;
}

i32 compile_exp_null(gab_compiler *self, boolean assignable) {
  push_op(self, OP_PUSH_NULL);
  return COMP_OK;
}

i32 compile_exp_lmd(gab_compiler *self, boolean assignable) {
  s_i8 name = {0};

  if (compile_function(self, name, TOKEN_PIPE) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_def(gab_compiler *self, boolean assignable) {

  if (expect_token(self, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  s_i8 name = self->lex.previous_token_src;

  u8 local = add_local(self, name, 0);

  if (compile_definition(self, name) < 0)
    return COMP_ERR;

  initialize_local(self, local);

  gab_module_push_store_local(self->mod, local, self->previous_token,
                              self->line);

  return COMP_OK;
}

i32 compile_exp_arr(gab_compiler *self, boolean assignable) {
  return compile_array(self);
}

i32 compile_exp_spd(gab_compiler *self, boolean assignable) {
  if (compile_expression(self) < 0)
    return COMP_ERR;

  push_op(self, OP_SPREAD);
  // Want byte
  push_byte(self, 1);

  return VAR_RET;
}

i32 compile_exp_rec(gab_compiler *self, boolean assignable) {
  return compile_record(self, (s_i8){0});
}

i32 compile_exp_let(gab_compiler *self, boolean assignable) {

  u8 locals[16] = {0};

  u8 local_count = 0;

  i32 result;
  do {
    if (local_count == 16) {
      set_err(self, "Can't have more than 16 variables in one let declaration");
      return COMP_ERR;
    }

    if (expect_token(self, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = self->lex.previous_token_src;

    i32 result = resolve_id(self, self->lex.previous_token_src, NULL);

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
      set_err(self, "A variable with this name already exists");
      break;
    }

    default:
      set_err(self, "Unexpected result compiling let expression");
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
    set_err(self, "Variables must be initialized");
    return COMP_ERR;

  default:
    set_err(self, "Unexpected result compiling let expression");
    return COMP_ERR;
  }

  while (local_count--) {
    u8 local = locals[local_count];
    initialize_local(self, local);

    if (local_count > 0) {
      push_op(self, OP_POP_STORE_LOCAL);
      push_byte(self, local);
    } else {
      gab_module_push_store_local(self->mod, local, self->previous_token,
                                  self->line);
    }
  }

  return COMP_OK;
}

i32 compile_exp_idn(gab_compiler *self, boolean assignable) {
  s_i8 name = self->lex.previous_token_src;

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
    set_err(self, "Unexpected identifier");
    return COMP_ERR;
  }

  default:
    set_err(self, "Unexpected result compiling identifier");
    return COMP_ERR;
  }

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      set_err(self, "Invalid assignment target");
      return COMP_ERR;
    }

    gab_compile_frame *frame = peek_frame(self, 0);
    if (is_local_var) {
      if (!(frame->locals_flag[var] & FLAG_MUTABLE)) {
        set_err(self, "Variable is not mutable");
      }
    } else {
      if (!(frame->upvs_flag[var] & FLAG_MUTABLE)) {
        set_err(self, "Variable is not mutable");
      }
    }

    if (compile_expression(self) < 0)
      return COMP_ERR;

    if (is_local_var) {
      gab_module_push_store_local(self->mod, var, self->previous_token,
                                  self->line);
    } else {
      gab_module_push_store_upvalue(self->mod, var, self->previous_token,
                                    self->line);
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    if (is_local_var) {
      gab_module_push_load_local(self->mod, var, self->previous_token,
                                 self->line);
    } else {
      gab_module_push_load_upvalue(
          self->mod, var, self->previous_token, self->line,
          peek_frame(self, 0)->upvs_flag[var] & FLAG_MUTABLE);
    }
    break;

  default:
    set_err(self, "Unexpected result compiling identifier");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_idx(gab_compiler *self, boolean assignable) {
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
      set_err(self, "Invalid assignment target");
      return COMP_ERR;
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_op(self, OP_GET_INDEX);
    break;
  }

  default:
    set_err(self, "Unexpected result compiling index");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_rec_call(gab_compiler *self, boolean assignable) {
  if (compile_exp_rec(self, assignable) < 0)
    return COMP_ERR;

  gab_module_push_call(self->mod, 1, false, self->previous_token, self->line);

  return VAR_RET;
}

i32 compile_exp_call(gab_compiler *self, boolean assignable) {
  boolean vse = false;
  i32 result = 0;

  if (!match_token(self, TOKEN_RPAREN)) {
    result = compile_expressions(self, VAR_RET, &vse);

    if (result < 0)
      return COMP_ERR;
  }

  if (!vse && result > 16) {
    set_err(self, "Too many arguments to function call");
    return COMP_ERR;
  }

  gab_module_push_call(self->mod, result, vse, self->previous_token,
                       self->line);

  if (expect_token(self, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return VAR_RET;
}

i32 compile_exp_dot(gab_compiler *self, boolean assignable) {
  if (compile_property(self, assignable) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_glb(gab_compiler *self, boolean assignable) {
  u8 global;

  switch (resolve_id(self, s_i8_create((i8 *)"__global__", 10), &global)) {
  case COMP_RESOLVED_TO_LOCAL: {
    gab_module_push_load_local(self->mod, global, self->previous_token,
                               self->line);
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    gab_module_push_load_upvalue(self->mod, global, self->previous_token,
                                 self->line, false);
    break;
  }

  default:
    set_err(self, "Unexpected result compiling global");
    return COMP_ERR;
  }

  if (compile_property(self, assignable) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_mth(gab_compiler *self, boolean assignable) {
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
    set_err(self, "Too many arguments to method call");
    return COMP_ERR;
  }

  gab_module_push_call(self->mod, result, vse, self->previous_token,
                       self->line);

  return VAR_RET;
}

i32 compile_exp_and(gab_compiler *self, boolean assignable) {
  u64 end_jump = gab_module_push_jump(self->mod, OP_LOGICAL_AND,
                                      self->previous_token, self->line);

  if (compile_exp_prec(self, PREC_AND) < 0)
    return COMP_ERR;

  gab_module_patch_jump(self->mod, end_jump);

  return COMP_OK;
}

i32 compile_exp_or(gab_compiler *self, boolean assignable) {
  u64 end_jump = gab_module_push_jump(self->mod, OP_LOGICAL_OR,
                                      self->previous_token, self->line);

  if (compile_exp_prec(self, PREC_OR) < 0)
    return COMP_ERR;

  gab_module_patch_jump(self->mod, end_jump);

  return COMP_OK;
}

i32 compile_exp_prec(gab_compiler *self, gab_precedence prec) {

  if (eat_token(self) < 0)
    return COMP_ERR;

  gab_compile_rule rule = get_rule(self->previous_token);

  if (rule.prefix == NULL) {
    set_err(self, "Expected an expression");
    return COMP_ERR;
  }

  boolean assignable = prec <= PREC_ASSIGNMENT;

  i32 have = rule.prefix(self, assignable);
  if (have < 0)
    return COMP_ERR;

  u64 line = self->line;

  while (prec <= get_rule(self->current_token).prec) {

    if (have < 0)
      return COMP_ERR;

    if (eat_token(self) < 0)
      return COMP_ERR;

    rule = get_rule(self->previous_token);

    // Maybe try and see if the current token has a rule for prefix here?
    if (rule.infix == NULL) {
      // No infix rule for this token it might be a postfix though.

      if (rule.postfix == NULL) {
        set_err(self, "Unknown infix or postfix expression");
        return COMP_ERR;
      }

      have = rule.postfix(self, assignable);
    } else {
      // Treat this as an infix expression.
      have = rule.infix(self, assignable);
      line = self->line;
    }
  }

  // TODO: See if this actually does anything.
  if (assignable && match_token(self, TOKEN_EQUAL)) {
    set_err(self, "Invalid assignment target");
    return COMP_ERR;
  }

  return have;
}

i32 compile_exp_for(gab_compiler *self, boolean assignable) {
  down_scope(self);

  i32 iter = add_invisible_local(self);
  if (iter < 0)
    return COMP_ERR;

  u16 loop_locals = 0;
  i32 result;

  do {

    if (expect_token(self, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = self->lex.previous_token_src;

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

  u64 loop_start = self->mod->bytecode.len - 1;

  // Load the funciton.
  gab_module_push_load_local(self->mod, iter, self->previous_token, self->line);

  // Call the function, wanting loop_locals results.
  push_op(self, OP_CALL_0);
  push_byte(self, loop_locals);

  // Pop the results in reverse order, assigning them to each loop local.
  for (u8 ll = 0; ll < loop_locals; ll++) {
    push_op(self, OP_POP_STORE_LOCAL);
    push_byte(self, iter + loop_locals - ll);
  }

  gab_module_push_load_local(self->mod, iter + loop_locals,
                             self->previous_token, self->line);

  // Exit the for loop if the last loop local is false.
  u64 jump_start = gab_module_push_jump(self->mod, OP_JUMP_IF_FALSE,
                                        self->previous_token, self->line);

  if (optional_newline(self) < 0)
    return COMP_ERR;

  push_op(self, OP_POP);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  push_op(self, OP_POP);

  gab_module_push_loop(self->mod, loop_start, self->previous_token, self->line);

  gab_module_patch_jump(self->mod, jump_start);

  up_scope(self);

  return COMP_OK;
}

i32 compile_exp_whl(gab_compiler *self, boolean assignable) {
  u64 loop_start = self->mod->bytecode.len - 1;

  down_scope(self);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  if (optional_newline(self) < 0)
    return COMP_ERR;

  u64 jump = gab_module_push_jump(self->mod, OP_POP_JUMP_IF_FALSE,
                                  self->previous_token, self->line);

  if (compile_expression(self) < 0)
    return COMP_ERR;

  up_scope(self);

  gab_module_push_loop(self->mod, loop_start, self->previous_token, self->line);

  gab_module_patch_jump(self->mod, jump);

  push_op(self, OP_PUSH_NULL);

  return COMP_OK;
}

i32 compile_exp_rtn(gab_compiler *self, boolean assignable) {
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
    set_err(self, "Tried to return too many expressions");
    return COMP_ERR;
  }

  gab_module_push_return(self->mod, result, vse, self->previous_token,
                         self->line);
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
const gab_compile_rule gab_compiler_rules[] = {
    PREFIX(let),                       // LET
    PREFIX(mch),                       // MATCH
    PREFIX(if),                        // IF
    NONE(),                            // THEN
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

gab_compile_rule get_rule(gab_token k) { return gab_compiler_rules[k]; }

gab_obj_closure *compile(gab_compiler *self, s_i8 name) {

  down_frame(self, name);

  gab_lexer_create(&self->lex, self->mod->source);

  self->mod->source_lines = self->lex.source_lines;

  if (eat_token(self) == COMP_ERR)
    return NULL;

  initialize_local(self,
                   add_local(self, s_i8_create((i8 *)"__global__", 10), 0));

  if (compile_block_body(self) == COMP_ERR)
    return NULL;

  gab_obj_function *main_func =
      gab_obj_function_create(self->mod->engine, name);

  gab_engine_add_constant(self->mod->engine, GAB_VAL_OBJ(main_func));

  main_func->module = self->mod;
  main_func->narguments = 1;
  main_func->nlocals = peek_frame(self, 0)->deepest_local;

  gab_obj_closure *main =
      gab_obj_closure_create(self->mod->engine, main_func, NULL);

  up_frame(self);

  push_op(self, OP_RETURN_1);

  return main;
  ;
}

gab_result *gab_engine_compile(gab_engine *eng, s_i8 name, s_i8 src, u8 flags) {

  gab_module *module = gab_engine_add_module(eng, name, src);

  gab_compiler compiler;
  gab_compiler_create(&compiler, module);

  gab_obj_closure *main = compile(&compiler, name);

  if (main == NULL) {
    gab_result *error = gab_compile_fail(&compiler, compiler.error);
    return error;
  }

  if (flags & GAB_FLAG_DUMP_BYTECODE) {
    gab_module_dump(module, name);
  }

  return gab_compile_success(main);
}
