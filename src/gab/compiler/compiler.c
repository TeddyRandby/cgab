#include "compiler.h"
#include "module.h"
#include "object.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void gab_compiler_create(gab_compiler *self) {
  self->scope_depth = 0;
  self->frame_count = 0;
  self->error = NULL;
  self->mod = NULL;

  memset(self->frames, 0, sizeof(self->frames));
}

// Helper macros for creating the specialized instructions
#define MAKE_RETURN(n) (OP_RETURN_1 + (n - 1))
#define MAKE_CALL(n) (OP_CALL_0 + (n))
#define MAKE_STORE_LOCAL(n) (OP_STORE_LOCAL_0 + (n))
#define MAKE_LOAD_LOCAL(n) (OP_LOAD_LOCAL_0 + (n))
#define MAKE_STORE_UPVALUE(n) (OP_STORE_UPVALUE_0 + (n))
#define MAKE_LOAD_UPVALUE(n) (OP_LOAD_UPVALUE_0 + (n))

// A positive result is known to be OK, and can carry a value.
typedef enum comp_result {
  COMP_ERR = -1,
  COMP_TOKEN_MATCH = -2,
  COMP_TOKEN_NO_MATCH = -3,
  COMP_LOCAL_NOT_FOUND = -4,
  COMP_UPVALUE_NOT_FOUND = -5,
  COMP_ID_NOT_FOUND = -6,
  COMP_RESOLVED_TO_LOCAL = -7,
  COMP_RESOLVED_TO_UPVALUE = -8,
  COMP_FIN = -9,
  COMP_MAX = INT32_MAX,
} comp_result;

// This macro doesn't do anything, but it helps communicate intent.
#define COMP_OK_VALUE(n) n
#define COMP_OK 1

#define ASSERT_NOT_ERR(exp)                                                    \
  if (exp == COMP_ERR) {                                                       \
    return COMP_ERR;                                                           \
  }

#define ASSERT_NOT_ERR_FIN(exp)                                                \
  if (exp == COMP_ERR exp == COMP_FIN) {                                       \
    return COMP_ERR;                                                           \
  }
// Helper macros for comp_result error handling
#define ASSIGN_HAS_MATCH(var, token)                                           \
  ((var = match_and_eat_token(self, token)) == COMP_TOKEN_MATCH)

#define ASSIGN_SKIP_NEWLINES(var) (result = skip_newlines(self))

typedef struct exp_list {
  u8 have;
  boolean is_var;
} exp_list;

static boolean match_token(gab_compiler *self, gab_token tok);
static comp_result eat_token(gab_compiler *self);

void error(gab_compiler *self, gab_result *err) {
  if (self->error == NULL) {
    gab_lexer_finish_line(&self->lex);
    self->error = err;
  } else {
    gab_result_destroy(err);
  }
}

//------------------- Token Helpers -----------------------
// Can't return an error.
static boolean match_token(gab_compiler *self, gab_token tok) {
  return self->current_token == tok;
}

// Returns less than 0 if there was an error, greater than 0 otherwise.
static comp_result eat_token(gab_compiler *self) {
  self->previous_token = self->current_token;
  self->current_token = gab_lexer_next(&self->lex);
  self->line = self->lex.current_row;

  if (match_token(self, TOKEN_ERROR)) {
    error(self, gab_compile_fail(self, self->lex.error_msg));
    return COMP_ERR;
  }

  return COMP_OK;
}

static comp_result match_and_eat_token(gab_compiler *self, gab_token tok) {
  if (!match_token(self, tok))
    return COMP_TOKEN_NO_MATCH;

  ASSERT_NOT_ERR(eat_token(self));

  return COMP_TOKEN_MATCH;
}

static comp_result expect_token(gab_compiler *self, gab_token tok) {
  i32 result;
  switch (result = match_and_eat_token(self, tok)) {

  // Return an error because the tokens didn't match
  case COMP_TOKEN_NO_MATCH: {
    eat_token(self);
    error(self, gab_compile_fail(self, "Unexpected Token"));
    return COMP_ERR;
  }

  default:
    return result;
  }
}

static u16 add_constant(gab_compiler *self, gab_value value) {
  u16 index = gab_engine_add_constant(self->mod->engine, value);
  return index;
}

//----------------- 'PUSH' helpers ----------------------
/*
    These helper functions emit bytecode into the module.
*/

static void push_byte(gab_compiler *self, u8 byte) {
  gab_module_push_byte(self->mod, byte);
  v_u8_push(&self->mod->tokens, self->previous_token);
  v_u64_push(&self->mod->lines, self->line);
}

static void push_bytes(gab_compiler *self, u8 byte_1, u8 byte_2) {
  push_byte(self, byte_1);
  push_byte(self, byte_2);
}

static void push_short(gab_compiler *self, u16 val) {
  push_bytes(self, (val >> 8) & 0xff, val & 0xff);
}

static inline void push_load_local(gab_compiler *self, u8 local) {
  if (local < 9) {
    push_byte(self, MAKE_LOAD_LOCAL(local));
  } else {
    push_bytes(self, OP_LOAD_LOCAL, local);
  }
}

static inline void push_store_local(gab_compiler *self, u8 local) {
  if (local < 9) {
    push_byte(self, MAKE_STORE_LOCAL(local));
  } else {
    push_bytes(self, OP_STORE_LOCAL, local);
  }
}

static inline void push_load_upvalue(gab_compiler *self, u8 upvalue) {
  if (upvalue < 9) {
    push_byte(self, MAKE_LOAD_UPVALUE(upvalue));
  } else {
    push_bytes(self, OP_LOAD_UPVALUE, upvalue);
  }
}

static inline void push_store_upvalue(gab_compiler *self, u8 upvalue) {
  if (upvalue < 9) {
    push_byte(self, MAKE_STORE_UPVALUE(upvalue));
  } else {
    push_bytes(self, OP_STORE_UPVALUE, upvalue);
  }
}

static void push_cache(gab_compiler *self) {
  // Push 8 bytes of space.
  push_bytes(self, 0, 0);
  push_bytes(self, 0, 0);
  push_bytes(self, 0, 0);
  push_bytes(self, 0, 0);
}

static u64 push_jump(gab_compiler *self, u8 op) {
  push_byte(self, op);
  push_bytes(self, 0xff, 0xff);
  return self->mod->bytecode.size - 2;
}

static comp_result patch_jump(gab_compiler *self, u64 dist) {
  i32 jump = self->mod->bytecode.size - dist - 2;

  if (jump > UINT16_MAX) {
    error(self, gab_compile_fail(self, "Tried to jump over too much code"));
    return COMP_ERR;
  }

  v_u8_set(&self->mod->bytecode, dist, (jump >> 8) & 0xff);
  v_u8_set(&self->mod->bytecode, dist + 1, jump & 0xff);

  return COMP_OK;
}

static void patch_call(gab_compiler *self, u8 want) {
  u8 instr = v_u8_val_at(&self->mod->bytecode, self->mod->bytecode.size - 2);
  if (instr >= OP_CALL_0 && instr <= OP_CALL_16) {
    v_u8_set(&self->mod->bytecode, self->mod->bytecode.size - 1, want);
  }
}

static comp_result push_loop(gab_compiler *self, u64 dist) {
  i32 jump = self->mod->bytecode.size - dist + 2;

  push_byte(self, OP_LOOP);

  if (jump > UINT16_MAX) {
    error(self, gab_compile_fail(self, "Tried to jump over too much code"));
    return COMP_ERR;
  }

  push_byte(self, (jump >> 8) & 0xff);
  push_byte(self, jump & 0xff);
  return COMP_OK;
}

static comp_result push_return(gab_compiler *self, exp_list result) {
  if (result.is_var) {
    push_bytes(self, OP_VARRETURN, result.have);
  } else {
    if (result.have > FRET_MAX) {
      error(self, gab_compile_fail(
                      self, "Functions cannot return more than 16 values"));
      return COMP_ERR;
    }

    push_byte(self, MAKE_RETURN(result.have));
  }

  return COMP_OK;
}

static comp_result push_call(gab_compiler *self, exp_list args) {
  if (args.is_var) {
    push_bytes(self, OP_VARCALL, args.have);
  } else {
    if (args.have > FARG_MAX) {
      error(self, gab_compile_fail(
                      self, "Functions cannot take more than 16 arguments"));
      return COMP_ERR;
    }

    push_byte(self, MAKE_CALL(args.have));
  }
  // Want byte
  // This can be patched later, but it is assumed to be one.
  push_byte(self, 1);
  return COMP_OK;
}

//--------------------- Local Variable Helpers --------------
static gab_compile_frame *peek_frame(gab_compiler *self, u32 depth) {
  return &self->frames[self->frame_count - depth - 1];
}

static void initialize_local(gab_compiler *self, u8 local) {
  peek_frame(self, 0)->locals_depth[local] = self->scope_depth;
}

static comp_result add_invisible_local(gab_compiler *self) {
  gab_compile_frame *frame = peek_frame(self, 0);
  if (frame->local_count == LOCAL_MAX) {
    error(self, gab_compile_fail(
                    self, "Can't have more than 255 variables in a function"));
    return COMP_ERR;
  }
  u8 local = frame->local_count;
  frame->locals_name[local] = s_u8_ref_create_cstr("");
  initialize_local(self, local);

  if (++frame->local_count > frame->deepest_local)
    frame->deepest_local = frame->local_count;

  return COMP_OK_VALUE(local);
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static comp_result add_local(gab_compiler *self, s_u8_ref name) {
  gab_compile_frame *frame = peek_frame(self, 0);
  if (frame->local_count == LOCAL_MAX) {
    error(self, gab_compile_fail(
                    self, "Can't have more than 255 variables in a function"));
    return COMP_ERR;
  }

  u8 local = frame->local_count;
  frame->locals_name[local] = name;
  frame->locals_depth[local] = -1;
  frame->locals_captured[local] = false;

  if (++frame->local_count > frame->deepest_local)
    frame->deepest_local = frame->local_count;

  return COMP_OK_VALUE(local);
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * upvalue.
 */
static comp_result add_upvalue(gab_compiler *self, u32 depth, u8 index,
                               boolean is_local) {
  gab_compile_frame *frame = peek_frame(self, depth);
  u16 count = frame->upv_count;

  // Don't pull redundant upvalues
  for (int i = 0; i < count; i++) {
    if (frame->upvs_index[i] == index && frame->upvs_is_local[i] == is_local) {
      return i;
    }
  }

  if (count == UPVALUE_MAX) {
    error(self,
          gab_compile_fail(
              self, "Can't capture more than 255 variables in one closure"));
    return COMP_ERR;
  }

  frame->upvs_index[count] = index;
  frame->upvs_is_local[count] = is_local;
  frame->upv_count++;
  return COMP_OK_VALUE(count);
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_LOCAL_NOT_FOUND if no matching local is found,
 * and otherwise the offset of the local.
 */
static comp_result resolve_local(gab_compiler *self, s_u8_ref name, u32 depth) {
  for (i32 local = peek_frame(self, depth)->local_count - 1; local >= 0;
       local--) {
    if (s_u8_ref_match(name, peek_frame(self, depth)->locals_name[local])) {
      if (peek_frame(self, depth)->locals_depth[local] == -1) {
        error(self,
              gab_compile_fail(
                  self, "Can't reference a variable before it is initialized"));
        return COMP_ERR;
      }
      return COMP_OK_VALUE(local);
    }
  }
  return COMP_LOCAL_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_UPVALUE_NOT_FOUND if no matching upvalue is found,
 * and otherwise the offset of the upvalue.
 */
static comp_result resolve_upvalue(gab_compiler *self, s_u8_ref name,
                                   u32 depth) {
  // Base case, hopefully conversion doesn't cause issues
  i32 frame_index = self->frame_count - depth - 1;
  if (frame_index < 0) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  i32 local = resolve_local(self, name, depth);
  if (local >= 0) {
    /* printf("Found local in slot %d\n", local); */
    peek_frame(self, depth)->locals_captured[local] = true;
    return COMP_OK_VALUE(add_upvalue(self, depth - 1, (u8)local, true));
  }

  i32 upvalue = resolve_upvalue(self, name, depth + 1);
  if (upvalue >= 0) {
    return COMP_OK_VALUE(add_upvalue(self, depth - 1, (u8)upvalue, false));
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_REGABVED_TO_LOCAL if the id was a local, and
 * COMP_REGABVED_TO_UPVALUE if the id was an upvalue.
 */
static comp_result resolve_id(gab_compiler *self, s_u8_ref name, u8 *value_in) {

  i32 arg = resolve_local(self, name, 0);
  ASSERT_NOT_ERR(arg);

  if (arg == COMP_LOCAL_NOT_FOUND) {

    arg = resolve_upvalue(self, name, 0);
    ASSERT_NOT_ERR(arg);

    if (arg == COMP_UPVALUE_NOT_FOUND)
      return COMP_ID_NOT_FOUND;

    *value_in = (u8)arg;
    return COMP_RESOLVED_TO_UPVALUE;
  }

  *value_in = (u8)arg;
  return COMP_RESOLVED_TO_LOCAL;
}

static void down_scope(gab_compiler *self) { self->scope_depth++; }

static void up_scope(gab_compiler *self) {
  self->scope_depth--;

  gab_compile_frame *frame = peek_frame(self, 0);
  while (frame->local_count > 1 &&
         frame->locals_depth[frame->local_count - 1] > self->scope_depth) {
    frame->local_count--;
  }
}

static void down_frame(gab_compiler *self, s_u8_ref name) {
  self->frame_count++;
  // The first local in any given frame is reserved for the function being
  // called. It is immutable.
  peek_frame(self, 0)->local_count = 0;
  peek_frame(self, 0)->upv_count = 0;
  peek_frame(self, 0)->deepest_local = 0;
  peek_frame(self, 0)->function_name = name;

  i32 loc = add_local(self, name);
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
comp_result compile_exp_prec(gab_compiler *self, gab_precedence prec);
comp_result compile_expressions(gab_compiler *self, u8 want, exp_list *out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

/* Returns COMP_ERR if an error was encountered, and otherwise the offset of the
 * local.
 */
static comp_result compile_local(gab_compiler *self, s_u8_ref name) {

  for (i32 local = peek_frame(self, 0)->local_count - 1; local >= 0; local--) {
    if (peek_frame(self, 0)->locals_depth[local] != -1 &&
        peek_frame(self, 0)->locals_depth[local] < self->scope_depth) {
      break;
    }

    if (s_u8_ref_match(name, peek_frame(self, 0)->locals_name[local])) {
      error(self,
            gab_compile_fail(
                self, "There is already a variable with this name in scope"));
      return COMP_ERR;
    }
  }

  return COMP_OK_VALUE(add_local(self, name));
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
comp_result compile_function_args(gab_compiler *self,
                                  gab_obj_function *function) {

  i32 result;

  do {
    function->narguments++;
    if (function->narguments == FARG_MAX) {
      s_u8 *arg = s_u8_create_sref(function->name);
      error(self, gab_compile_fail(
                      self, "Functions can't have more than 16 parameters"));
      return COMP_ERR;
    }

    ASSERT_NOT_ERR(expect_token(self, TOKEN_IDENTIFIER));

    s_u8_ref name = self->lex.previous_token_src;
    i32 local = compile_local(self, name);
    ASSERT_NOT_ERR(local);

    // Function arguments are "initialized"
    // here as well.
    initialize_local(self, local);

  } while (ASSIGN_HAS_MATCH(result, TOKEN_COMMA));

  // only return false if there was an error.
  ASSERT_NOT_ERR(result);

  return COMP_OK;
}

static inline comp_result skip_newlines(gab_compiler *self) {
  while (match_token(self, TOKEN_NEWLINE)) {
    ASSERT_NOT_ERR(eat_token(self));
  }

  if (match_token(self, TOKEN_EOF))
    return COMP_FIN;

  return COMP_OK;
}

static inline comp_result optional_newline(gab_compiler *self) {
  return match_and_eat_token(self, TOKEN_NEWLINE);
}

static inline comp_result expect_newline(gab_compiler *self) {
  if (match_token(self, TOKEN_NEWLINE)) {
    ASSERT_NOT_ERR(eat_token(self));
  }

  if (match_token(self, TOKEN_EOF))
    return COMP_FIN;

  return COMP_OK;
}

comp_result compile_block_body(gab_compiler *self, u8 want, exp_list *exps) {

  comp_result result;

  ASSERT_NOT_ERR(skip_newlines((self)));

  if (match_token(self, TOKEN_EOF))
    return COMP_FIN;

  ASSERT_NOT_ERR(compile_expressions(self, want, exps));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_NEWLINE));

  if (match_token(self, TOKEN_EOF))
    return COMP_FIN;

  while (!match_token(self, TOKEN_END)) {

    push_byte(self, OP_POP);

    ASSERT_NOT_ERR(skip_newlines((self)));

    if (match_token(self, TOKEN_EOF))
      return COMP_FIN;

    ASSERT_NOT_ERR(compile_expressions(self, want, exps));

    ASSERT_NOT_ERR(expect_token(self, TOKEN_NEWLINE));

    if (match_token(self, TOKEN_EOF))
      return COMP_FIN;
  }

  ASSERT_NOT_ERR(expect_token(self, TOKEN_END));

  return COMP_OK;
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
comp_result compile_function_body(gab_compiler *self,
                                  gab_obj_function *function, u64 skip_jump) {
  // Start compiling the function's bytecode.
  // You can't just store a pointer to the beginning of the code
  // Because if the vector is EVER resized,
  // Then all your functions are suddenly invalid.
  function->offset = self->mod->bytecode.size;

  exp_list ret_value;
  ASSERT_NOT_ERR(compile_expressions(self, VAR_RET, &ret_value));
  ASSERT_NOT_ERR(push_return(self, ret_value));

  // Update the functions upvalue state and pop the function's compile_frame
  gab_compile_frame *frame = peek_frame(self, 0);

  function->module = self->mod;
  function->nupvalues = frame->upv_count;
  // Deepest local - args - caller = number of locals declared
  function->nlocals = frame->deepest_local - function->narguments - 1;

  // Patch the jump to skip over the function body
  ASSERT_NOT_ERR(patch_jump(self, skip_jump));

  // Push a closure wrapping the function
  push_byte(self, OP_CLOSURE);
  push_short(self, add_constant(self, GAB_VAL_OBJ(function)));

  for (int i = 0; i < function->nupvalues; i++) {
    push_byte(self, frame->upvs_is_local[i]);
    push_byte(self, frame->upvs_index[i]);
  }

  return COMP_OK;
}

comp_result compile_function(gab_compiler *self, s_u8_ref name,
                             gab_token closing) {
  u64 skip_jump = push_jump(self, OP_JUMP);

  // Doesnt need a pop scope because the scope is popped with the callframe.
  down_scope(self);

  gab_obj_function *function = gab_obj_function_create(self->mod->engine, name);

  down_frame(self, name);

  if (!match_token(self, closing)) {
    ASSERT_NOT_ERR(compile_function_args(self, function));
  }

  ASSERT_NOT_ERR(expect_token(self, closing));
  ASSERT_NOT_ERR(expect_token(self, TOKEN_COLON));

  ASSERT_NOT_ERR(compile_function_body(self, function, skip_jump));

  function->nlocals = peek_frame(self, 0)->deepest_local - function->narguments;

  up_frame(self);

  return COMP_OK;
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
comp_result compile_expressions(gab_compiler *self, u8 want, exp_list *out) {

  u8 have = 0;
  boolean is_var;

  i32 result;
  do {
    comp_result exp_count = compile_exp_prec(self, PREC_ASSIGNMENT);
    ASSERT_NOT_ERR(exp_count);
    // We successfully compiled an expression
    is_var = exp_count == VAR_RET;
    have++;
  } while (ASSIGN_HAS_MATCH(result, TOKEN_COMMA));

  ASSERT_NOT_ERR(result);

  if (have > want) {
    error(self, gab_compile_fail(self, "Too many expressions in list"));
    return COMP_ERR;
  }

  if (is_var) {
    have--; // Don't count the variable expression itself.

    // If our expression is variable
    if (want == VAR_RET || want > have) {
      // If we want a variable number or more than we have
      // If the expression is variable because it ends
      // with a function call, try to patch that call to want
      // either variable or the additional number of exps.
      patch_call(self, want - have);
    }
  } else {
    // If our expression is not variable
    if (want != VAR_RET) {
      // If we want a fixed number of expressions
      while (have < want) {
        // While we have fewer expressions than we want, push nulls.
        push_byte(self, OP_PUSH_NULL);
        have++;
      }
    }
  }

  // If we have an out argument, set its values.
  if (out != NULL) {
    out->have = have;
    out->is_var = is_var;
  }

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * constant.
 */
comp_result compile_id_constant(gab_compiler *self) {
  ASSERT_NOT_ERR(expect_token(self, TOKEN_IDENTIFIER));

  gab_obj_string *obj =
      gab_obj_string_create(self->mod->engine, self->lex.previous_token_src);

  return COMP_OK_VALUE(add_constant(self, GAB_VAL_OBJ(obj)));
}

comp_result compile_property(gab_compiler *self, boolean assignable) {
  i32 prop = compile_id_constant(self);
  ASSERT_NOT_ERR(prop);

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_TOKEN_MATCH: {
    if (!assignable) {
      error(self, gab_compile_fail(self, "Expression is not assignable"));
      return COMP_ERR;
    }

    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
    push_byte(self, OP_SET_PROPERTY);
    push_short(self, prop);
    // The shape at this get
    push_cache(self);
    // The cache'd offset
    push_short(self, 0);

    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_byte(self, OP_GET_PROPERTY);
    push_short(self, prop);
    // The shape at this get
    push_cache(self);
    // The cache'd offset
    push_short(self, 0);
    break;
  }

  default:
    error(self, gab_compile_fail(
                    self, "Unexpected result compiling property access"));
    return COMP_ERR;
  }

  return COMP_OK;
}

comp_result compile_lst_internal_item(gab_compiler *self, u8 index) {

  // Push the key, which is just the index.
  push_byte(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_NUMBER(index)));

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  return COMP_OK;
}

comp_result compile_lst_internals(gab_compiler *self) {
  u8 size = 0;

  ASSERT_NOT_ERR(skip_newlines(self));
  while (!match_token(self, TOKEN_RBRACE)) {

    ASSERT_NOT_ERR(compile_lst_internal_item(self, size));

    if (size == UINT8_MAX) {
      error(self,
            gab_compile_fail(
                self, "Can't initialize an object with more than 255 values"));
      return COMP_ERR;
    }

    ASSERT_NOT_ERR(skip_newlines(self));
    size++;
  }

  ASSERT_NOT_ERR(expect_token(self, TOKEN_RBRACE));
  return COMP_OK_VALUE(size);
}

// Forward decl
comp_result compile_definition(gab_compiler *self, s_u8_ref name);

comp_result compile_obj_internal_item(gab_compiler *self) {

  if (match_and_eat_token(self, TOKEN_IDENTIFIER) == COMP_TOKEN_MATCH) {

    // Get the string for the key and push the key.
    s_u8_ref name = self->lex.previous_token_src;

    gab_obj_string *obj =
        gab_obj_string_create(self->mod->engine, self->lex.previous_token_src);

    // Push the constant onto the stack.
    push_byte(self, OP_CONSTANT);
    push_short(self, add_constant(self, GAB_VAL_OBJ(obj)));

    // Compile the expression if theres a colon, or look for a local with
    // the name and use that as the value.
    switch (match_and_eat_token(self, TOKEN_EQUAL)) {

    case COMP_TOKEN_MATCH: {
      ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
      return COMP_OK;
    }

    case COMP_TOKEN_NO_MATCH: {
      u8 value_in;
      boolean mut_in;

      i32 result = resolve_id(self, name, &value_in);
      ASSERT_NOT_ERR(result);

      if (result == COMP_RESOLVED_TO_LOCAL) {
        // If the id exists and is local, get it.
        push_load_local(self, value_in);
      } else if (result == COMP_RESOLVED_TO_UPVALUE) {
        // If the id exists and is an upvalue, get it.
        push_load_upvalue(self, value_in);
      } else if (result == COMP_ID_NOT_FOUND) {
        // If the id doesn't exist, push true.
        push_byte(self, OP_PUSH_TRUE);
      }
      return COMP_OK;
    }

    default:
      goto fin;
    }
  }

  if (match_and_eat_token(self, TOKEN_LBRACE) == COMP_TOKEN_MATCH) {

    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

    ASSERT_NOT_ERR(expect_token(self, TOKEN_RBRACE));

    ASSERT_NOT_ERR(expect_token(self, TOKEN_COLON));

    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
    return COMP_OK;
  }

  if (match_and_eat_token(self, TOKEN_DEF) == COMP_TOKEN_MATCH) {
    i32 prop = compile_id_constant(self);
    ASSERT_NOT_ERR(prop);

    s_u8_ref name = self->lex.previous_token_src;

    push_byte(self, OP_CONSTANT);
    push_short(self, prop);

    ASSERT_NOT_ERR(compile_definition(self, name));
    return COMP_OK;
  }

fin:
  error(self, gab_compile_fail(self, "Unexpected token in object item"));
  return COMP_ERR;
}

comp_result compile_obj_internals(gab_compiler *self) {
  u8 size = 0;

  ASSERT_NOT_ERR(skip_newlines(self));
  while (match_and_eat_token(self, TOKEN_RBRACK) == COMP_TOKEN_NO_MATCH) {

    ASSERT_NOT_ERR(compile_obj_internal_item(self));

    if (size == UINT8_MAX) {
      error(self,
            gab_compile_fail(
                self, "Can't initialize an object with more than 255 values"));
      return COMP_ERR;
    }

    ASSERT_NOT_ERR(skip_newlines(self));

    size++;
  };

  return COMP_OK_VALUE(size);
}

comp_result compile_definition(gab_compiler *self, s_u8_ref name) {
  if (match_and_eat_token(self, TOKEN_LPAREN) == COMP_TOKEN_MATCH) {

    ASSERT_NOT_ERR(compile_function(self, name, TOKEN_RPAREN));

    return COMP_OK;
  }

  if (match_and_eat_token(self, TOKEN_LBRACK) == COMP_TOKEN_MATCH) {
    i32 size = compile_obj_internals(self);
    ASSERT_NOT_ERR(size);

    push_bytes(self, OP_OBJECT, size);

    return COMP_OK;
  }

  if (match_and_eat_token(self, TOKEN_LBRACE) == COMP_TOKEN_MATCH) {
    i32 size = compile_lst_internals(self);
    ASSERT_NOT_ERR(size);

    push_bytes(self, OP_OBJECT, size);

    return COMP_OK;
  }

  error(self, gab_compile_fail(self, "Unknown definition"));
  return COMP_ERR;
}

//---------------- Compiling Expressions ------------------

comp_result compile_exp_blk(gab_compiler *self, boolean assignable) {

  down_scope(self);
  comp_result res = compile_block_body(self, 1, NULL);
  up_scope(self);

  ASSERT_NOT_ERR(res);
  if (res == COMP_FIN) {
    error(self, gab_compile_fail(self, "Unfinished do expression"));
    return COMP_ERR;
  }
  return COMP_OK;
}

comp_result compile_exp_let(gab_compiler *self, boolean assignable) {
  u8 value_in;

  boolean allow_new = true;

  u8 locals[16] = {0};
  boolean is_locals[16] = {0};
  boolean is_news[16] = {0};

  u8 local_count = 0;

  i32 comma_result;
  do {
    if (local_count == 16) {
      error(self,
            gab_compile_fail(
                self,
                "Can't have more than 16 variables in one let declaration"));
      return COMP_ERR;
    }

    ASSERT_NOT_ERR(expect_token(self, TOKEN_IDENTIFIER));

    s_u8_ref name = self->lex.previous_token_src;

    i32 result = resolve_id(self, self->lex.previous_token_src, &value_in);

    switch (result) {

    case COMP_ID_NOT_FOUND: {
      if (!allow_new) {
        error(self, gab_compile_fail(
                        self, "New declarations must occur before shadowing"));
        return COMP_ERR;
      }

      i32 loc = compile_local(self, name);
      ASSERT_NOT_ERR(loc);

      locals[local_count] = loc;
      is_news[local_count] = true;
      is_locals[local_count] = true;
      break;
    }

    case COMP_RESOLVED_TO_LOCAL: {
      allow_new = false;
      is_news[local_count] = false;

      locals[local_count] = value_in;
      is_locals[local_count] = true;

      break;
    }

    case COMP_RESOLVED_TO_UPVALUE: {
      allow_new = false;
      is_news[local_count] = false;

      locals[local_count] = value_in;
      is_locals[local_count] = false;
      break;
    }

    default:
      error(self, gab_compile_fail(
                      self, "Unexpected result compiling let expression"));
      return COMP_ERR;
    }

    local_count++;

  } while (ASSIGN_HAS_MATCH(comma_result, TOKEN_COMMA));

  ASSERT_NOT_ERR(comma_result);

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_TOKEN_MATCH: {
    ASSERT_NOT_ERR(compile_expressions(self, local_count, NULL));
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    for (u8 i = 0; i < local_count; i++) {
      push_byte(self, OP_PUSH_NULL);
    }
    break;

  default:
    error(self,
          gab_compile_fail(self, "Unexpected result compiling let expression"));
    return COMP_ERR;
  }

  while (local_count--) {
    u8 local = locals[local_count];
    u8 is_new = is_news[local_count];
    u8 is_local = is_locals[local_count];

    if (is_new) {
      // Initialize all the new locals.
      initialize_local(self, local);
    }

    if (local_count > 0) {
      if (is_local) {
        push_bytes(self, OP_POP_STORE_LOCAL, local);
      } else {
        push_bytes(self, OP_POP_STORE_UPVALUE, local);
      }
    } else {
      if (is_local) {
        push_store_local(self, local);
      } else {
        push_store_upvalue(self, local);
      }
    }
  }

  return COMP_OK;
}

comp_result compile_exp_if(gab_compiler *self, boolean assignable) {

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_COLON));
  ASSERT_NOT_ERR(optional_newline(self));

  u64 then_jump = push_jump(self, OP_POP_JUMP_IF_FALSE);

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  u64 else_jump = push_jump(self, OP_JUMP);
  ASSERT_NOT_ERR(patch_jump(self, then_jump));

  switch (match_and_eat_token(self, TOKEN_ELSE)) {
  case COMP_TOKEN_MATCH:
    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
    break;

  case COMP_TOKEN_NO_MATCH:
    push_byte(self, OP_PUSH_NULL);
    break;

  default:
    error(self,
          gab_compile_fail(self, "Unexpected result compiling if expression"));
    return COMP_ERR;
  }

  patch_jump(self, else_jump);

  return COMP_OK;
}

comp_result compile_exp_mch(gab_compiler *self, boolean assignable) {
  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_COLON));
  ASSERT_NOT_ERR(optional_newline(self));

  u64 next = 0;

  v_u64 done_jumps;
  v_u64_create(&done_jumps);

  while (match_and_eat_token(self, TOKEN_IF) == COMP_TOKEN_MATCH) {
    if (next != 0)
      ASSERT_NOT_ERR(patch_jump(self, next));

    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

    push_byte(self, OP_MATCH);

    next = push_jump(self, OP_POP_JUMP_IF_FALSE);

    ASSERT_NOT_ERR(expect_token(self, TOKEN_FAT_ARROW));

    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

    // Push a jump out of the match statement at the end of every case.
    v_u64_push(&done_jumps, push_jump(self, OP_JUMP));

    ASSERT_NOT_ERR(expect_token(self, TOKEN_NEWLINE));
  }

  // If none of the cases match, the last jump should end up here.
  ASSERT_NOT_ERR(patch_jump(self, next));

  if (match_and_eat_token(self, TOKEN_ELSE) == COMP_TOKEN_MATCH) {
    ASSERT_NOT_ERR(expect_token(self, TOKEN_FAT_ARROW));
    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
  } else {
    push_byte(self, OP_PUSH_NULL);
  }

  for (i32 i = 0; i < done_jumps.size; i++) {
    // Patch all the jumps to the end of the match expression.
    patch_jump(self, v_u64_val_at(&done_jumps, i));
  }

  return COMP_OK;
}

/*
 * Postfix assert expression.
 */
comp_result compile_exp_asrt(gab_compiler *self, boolean assignable) {
  push_byte(self, OP_ASSERT);
  return COMP_OK;
}

/*
 * Postfix type expression.
 */
comp_result compile_exp_type(gab_compiler *self, boolean assignable) {
  push_byte(self, OP_TYPE);
  return COMP_OK;
}

/*
 * Infix is expression.
 */
comp_result compile_exp_is(gab_compiler *self, boolean assignable) {
  push_byte(self, OP_TYPE);

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  push_byte(self, OP_EQUAL);

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
i32 compile_exp_bin(gab_compiler *self, boolean assignable) {
  gab_token op = self->previous_token;

  ASSERT_NOT_ERR(compile_exp_prec(self, get_rule(op).prec + 1));

  switch (op) {
  case TOKEN_MINUS: {
    push_byte(self, OP_SUBTRACT);
    break;
  }
  case TOKEN_PLUS: {
    push_byte(self, OP_ADD);
    break;
  }
  case TOKEN_STAR: {
    push_byte(self, OP_MULTIPLY);
    break;
  }
  case TOKEN_SLASH: {
    push_byte(self, OP_DIVIDE);
    break;
  }
  case TOKEN_PERCENT: {
    push_byte(self, OP_MODULO);
    break;
  }
  case TOKEN_DOT_DOT: {
    push_byte(self, OP_CONCAT);
    break;
  }
  case TOKEN_EQUAL_EQUAL: {
    push_byte(self, OP_EQUAL);
    break;
  }
  case TOKEN_LESSER: {
    push_byte(self, OP_LESSER);
    break;
  }
  case TOKEN_LESSER_EQUAL: {
    push_byte(self, OP_LESSER_EQUAL);
    break;
  }
  case TOKEN_GREATER: {
    push_byte(self, OP_GREATER);
    break;
  }
  case TOKEN_GREATER_EQUAL: {
    push_byte(self, OP_GREATER_EQUAL);
    break;
  }
  case TOKEN_AMPERSAND: {
    push_byte(self, OP_BITWISE_AND);
    break;
  }
  case TOKEN_PIPE: {
    push_byte(self, OP_BITWISE_OR);
    break;
  }
  default: {
    error(self, gab_compile_fail(self, "Unknown binary operator"));
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
comp_result compile_exp_una(gab_compiler *self, boolean assignable) {
  gab_token op = self->previous_token;

  ASSERT_NOT_ERR(compile_exp_prec(self, PREC_UNARY));

  switch (op) {
  case TOKEN_MINUS: {
    push_byte(self, OP_NEGATE);
    break;
  }
  case TOKEN_NOT: {
    push_byte(self, OP_NOT);
    break;
  }
  default: {
    s_u8 *tok = gab_token_to_string(op);
    error(self, gab_compile_fail(self, "Unknown unary operator"));
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Returns null if an error occured.
 */
s_u8 *parse_raw_str(gab_compiler *self, s_u8_ref raw_str) {
  // Skip the first '"'
  // The parsed string will be at most as long as the raw string.
  // (\n -> one char)
  char *buffer = CREATE_ARRAY(u8, raw_str.size + 1);
  u64 buf_end = 0;

  // Skip the first and last character

  for (u64 i = 1; i < raw_str.size - 1; i++) {
    u8 c = raw_str.data[i];

    if (c == '\\') {

      u8 code;
      switch (raw_str.data[i + 1]) {

      case 'n': {
        code = '\n';
        break;
      }
      case 't': {
        code = '\t';
        break;
      }
      case '{': {
        code = '{';
        break;
      }
      case '\\': {
        code = '\\';
        break;
      }
      default: {
        error(self,
              gab_compile_fail(self, "Invalid escape character in string"));
        DESTROY_ARRAY(buffer);
        return NULL;
      }
      }

      buffer[buf_end++] = code;
      i++;

    } else {
      buffer[buf_end++] = c;
    }
  }

  buffer[buf_end++] = '\0';

  s_u8 *result = s_u8_create_cstr(buffer);
  DESTROY_ARRAY(buffer);

  return result;
};

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
comp_result compile_exp_str(gab_compiler *self, boolean assignable) {

  s_u8_ref raw_token = self->lex.previous_token_src;

  s_u8 *parsed = parse_raw_str(self, raw_token);

  if (parsed == NULL) {
    error(self, gab_compile_fail(self, "String parsing failed"));
    return COMP_ERR;
  }

  gab_obj_string *obj =
      gab_obj_string_create(self->mod->engine, s_u8_ref_create_s_u8(parsed));

  s_u8_destroy(parsed);

  push_byte(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_OBJ(obj)));

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
comp_result compile_exp_itp(gab_compiler *self, boolean assignable) {

  ASSERT_NOT_ERR(compile_exp_str(self, assignable));

  if (match_token(self, TOKEN_INTERPOLATION_END)) {
    // Empty interpolation
    goto fin;
  }

  if (!match_token(self, TOKEN_INTERPOLATION)) {
    ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

    push_bytes(self, OP_STRINGIFY, OP_CONCAT);
  }

  i32 result;
  while (ASSIGN_HAS_MATCH(result, TOKEN_INTERPOLATION)) {
    ASSERT_NOT_ERR(compile_exp_str(self, assignable));

    if (match_token(self, TOKEN_INTERPOLATION_END)) {
      goto fin;
    }

    if (!match_token(self, TOKEN_INTERPOLATION)) {
      ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

      push_bytes(self, OP_STRINGIFY, OP_CONCAT);
    }

    // concat this to the long-running string.
    push_byte(self, OP_CONCAT);
  }

  ASSERT_NOT_ERR(result);

fin:
  ASSERT_NOT_ERR(expect_token(self, TOKEN_INTERPOLATION_END));

  ASSERT_NOT_ERR(compile_exp_str(self, assignable));

  // Concat the final string.
  push_byte(self, OP_CONCAT);
  return COMP_OK;
}

comp_result compile_exp_grp(gab_compiler *self, boolean assignable) {

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_RPAREN));

  return COMP_OK;
}

comp_result compile_exp_num(gab_compiler *self, boolean assignable) {
  f64 num = strtod((char *)self->lex.previous_token_src.data, NULL);
  push_byte(self, OP_CONSTANT);
  push_short(self, add_constant(self, GAB_VAL_NUMBER(num)));

  return COMP_OK;
}

comp_result compile_exp_bool(gab_compiler *self, boolean assignable) {
  push_byte(self,
            self->previous_token == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  return COMP_OK;
}

comp_result compile_exp_null(gab_compiler *self, boolean assignable) {
  push_byte(self, OP_PUSH_NULL);
  return COMP_OK;
}

comp_result compile_exp_lmd(gab_compiler *self, boolean assignable) {
  s_u8_ref name = s_u8_ref_create_cstr("anonymous");

  compile_function(self, name, TOKEN_PIPE);
  return COMP_OK;
}

comp_result compile_exp_def(gab_compiler *self, boolean assignable) {

  ASSERT_NOT_ERR(expect_token(self, TOKEN_IDENTIFIER));

  s_u8_ref name = self->lex.previous_token_src;

  u8 local = add_local(self, name);

  initialize_local(self, local);

  ASSERT_NOT_ERR(compile_definition(self, name));

  push_store_local(self, local);

  return COMP_OK;
}

comp_result compile_exp_lst(gab_compiler *self, boolean assignable) {

  comp_result size = compile_lst_internals(self);
  ASSERT_NOT_ERR(size);

  push_bytes(self, OP_OBJECT, size);
  return COMP_OK;
}

comp_result compile_exp_obj(gab_compiler *self, boolean assignable) {

  i32 size = compile_obj_internals(self);
  ASSERT_NOT_ERR(size);

  push_bytes(self, OP_OBJECT, size);
  return COMP_OK;
}

comp_result compile_exp_idn(gab_compiler *self, boolean assignable) {
  u8 value_in;
  s_u8_ref name = self->lex.previous_token_src;

  u8 var;
  boolean is_local_var;

  switch (resolve_id(self, name, &value_in)) {

  case COMP_ID_NOT_FOUND: {
    error(self, gab_compile_fail(self, "Unknown identifier"));
    return COMP_ERR;
  }

  case COMP_RESOLVED_TO_LOCAL: {
    var = value_in;
    is_local_var = true;
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    var = value_in;
    is_local_var = false;
    break;
  }

  default:
    error(self,
          gab_compile_fail(self, "Unexpected result compiling identifier"));
    return COMP_ERR;
  }

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_TOKEN_MATCH: {
    if (assignable) {
      ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

      if (is_local_var) {
        push_store_local(self, var);
      } else {
        push_store_upvalue(self, var);
      }

    } else {
      error(self, gab_compile_fail(self, "Invalid assignment target"));
      return COMP_ERR;
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    if (is_local_var) {
      push_load_local(self, var);
    } else {
      push_load_upvalue(self, var);
    }
    break;

  default:
    error(self,
          gab_compile_fail(self, "Unexpected result compiling identifier"));
    return COMP_ERR;
  }

  return COMP_OK;
}

comp_result compile_exp_idx(gab_compiler *self, boolean assignable) {
  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_RBRACE));

  switch (match_and_eat_token(self, TOKEN_EQUAL)) {

  case COMP_TOKEN_MATCH: {
    if (assignable) {
      ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
      push_byte(self, OP_SET_INDEX);
    } else {
      error(self, gab_compile_fail(self, "Invalid assignment target"));
      return COMP_ERR;
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_byte(self, OP_GET_INDEX);
    break;
  }

  default:
    error(self, gab_compile_fail(self, "Unexpected result compiling index"));
    return COMP_ERR;
  }

  return COMP_OK;
}

comp_result compile_exp_obj_call(gab_compiler *self, boolean assignable) {
  ASSERT_NOT_ERR(compile_exp_obj(self, assignable));

  ASSERT_NOT_ERR(push_call(self, (exp_list){.have = 1, .is_var = false}));

  return COMP_OK_VALUE(VAR_RET);
}

comp_result compile_exp_call(gab_compiler *self, boolean assignable) {
  exp_list args;

  if (!match_token(self, TOKEN_RPAREN)) {
    ASSERT_NOT_ERR(compile_expressions(self, VAR_RET, &args));
  } else {
    args.have = 0;
    args.is_var = false;
  }

  ASSERT_NOT_ERR(push_call(self, args));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_RPAREN));

  return COMP_OK_VALUE(VAR_RET);
}

comp_result compile_exp_dot(gab_compiler *self, boolean assignable) {

  ASSERT_NOT_ERR(compile_property(self, assignable));

  return COMP_OK;
}

comp_result compile_exp_glb(gab_compiler *self, boolean assignable) {
  u8 global;
  comp_result res =
      resolve_id(self, s_u8_ref_create_cstr("__global__"), &global);
  ASSERT_NOT_ERR(res);

  switch (res) {
  case COMP_RESOLVED_TO_LOCAL: {
    push_load_local(self, global);
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    push_load_upvalue(self, global);
    break;
  }

  default:
    error(self, gab_compile_fail(self, "Unexpected result compiling global"));
    return COMP_ERR;
  }

  ASSERT_NOT_ERR(compile_property(self, assignable));

  return COMP_OK;
}

comp_result compile_exp_mth(gab_compiler *self, boolean assignable) {
  // Compile the function that is being called on the top of the stack.
  ASSERT_NOT_ERR(compile_exp_prec(self, PREC_PROPERTY));

  /* Stack has now
    | method    |
    | receiver  |
  */

  // Swap the method and receiver
  push_byte(self, OP_SWAP);

  exp_list args;

  ASSERT_NOT_ERR(expect_token(self, TOKEN_LPAREN));

  if (!match_token(self, TOKEN_RPAREN)) {
    ASSERT_NOT_ERR(compile_expressions(self, VAR_RET, &args));
  } else {
    args.have = 0;
    args.is_var = false;
  }

  ASSERT_NOT_ERR(expect_token(self, TOKEN_RPAREN));

  /*
    | args..    |
    | receiver  |
    | method    |

   Stack has now
  */

  // Count the self argument
  args.have++;

  ASSERT_NOT_ERR(push_call(self, args));

  return COMP_OK_VALUE(VAR_RET);
}

comp_result compile_exp_and(gab_compiler *self, boolean assignable) {
  u64 end_jump = push_jump(self, OP_LOGICAL_AND);

  ASSERT_NOT_ERR(compile_exp_prec(self, PREC_AND));

  ASSERT_NOT_ERR(patch_jump(self, end_jump));
  return COMP_OK;
}

comp_result compile_exp_or(gab_compiler *self, boolean assignable) {
  u64 end_jump = push_jump(self, OP_LOGICAL_OR);

  ASSERT_NOT_ERR(compile_exp_prec(self, PREC_OR));

  ASSERT_NOT_ERR(patch_jump(self, end_jump));
  return COMP_OK;
}

comp_result compile_exp_prec(gab_compiler *self, gab_precedence prec) {

  ASSERT_NOT_ERR(eat_token(self));

  gab_compile_rule rule = get_rule(self->previous_token);

  if (rule.prefix == NULL) {
    error(self, gab_compile_fail(self, "Expected an expression"));
    return COMP_ERR;
  }

  boolean assignable = prec <= PREC_ASSIGNMENT;

  i32 have = rule.prefix(self, assignable);
  u64 line = self->line;

  ASSERT_NOT_ERR(have);

  while (prec <= get_rule(self->current_token).prec) {

    ASSERT_NOT_ERR(eat_token(self));

    rule = get_rule(self->previous_token);

    // Maybe try and see if the current token has a rule for prefix here?
    if (rule.infix == NULL) {
      // No infix rule for this token it might be a postfix though.

      if (rule.postfix == NULL) {
        error(self,
              gab_compile_fail(self, "Unknown infix or postfix expression"));
        return COMP_ERR;
      }

      have = rule.postfix(self, assignable);
      ASSERT_NOT_ERR(have);
    } else {
      // Treat this as an infix expression.
      have = rule.infix(self, assignable);
      line = self->line;
      ASSERT_NOT_ERR(have);
    }
  }

  // TODO: See if this actually does anything.
  if (assignable && match_token(self, TOKEN_EQUAL)) {
    error(self, gab_compile_fail(self, "Invalid assignment target"));
    return COMP_ERR;
  }

  return COMP_OK_VALUE(have);
}

comp_result compile_exp_for(gab_compiler *self, boolean assignable) {
  down_scope(self);

  i32 iter = add_invisible_local(self);
  ASSERT_NOT_ERR(iter);

  u16 loop_locals = 0;
  i32 result;
  do {
    ASSERT_NOT_ERR(expect_token(self, TOKEN_IDENTIFIER));
    s_u8_ref name = self->lex.previous_token_src;

    i32 loc = compile_local(self, name);
    ASSERT_NOT_ERR(loc);

    initialize_local(self, loc);
    loop_locals++;
  } while (ASSIGN_HAS_MATCH(result, TOKEN_COMMA));
  ASSERT_NOT_ERR(result);

  ASSERT_NOT_ERR(expect_token(self, TOKEN_IN));

  // This is the iterator function, store into iter local
  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));
  push_store_local(self, iter);

  u64 loop_start = self->mod->bytecode.size - 1;

  // Push the iterator and call it
  push_load_local(self, iter);
  push_bytes(self, OP_CALL_0, loop_locals);
  // Store the results of this call into the loop locals.
  for (u16 i = 0; i < loop_locals; i++) {
    push_bytes(self, OP_POP_STORE_LOCAL, iter + loop_locals - i);
  }

  // Load the last loop local
  push_load_local(self, iter + loop_locals);

  // Exit the for loop if the last loop local is false.
  u64 jump_start = push_jump(self, OP_JUMP_IF_FALSE);

  ASSERT_NOT_ERR(expect_token(self, TOKEN_COLON));
  ASSERT_NOT_ERR(optional_newline(self));

  push_byte(self, OP_POP);

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  push_byte(self, OP_POP);

  ASSERT_NOT_ERR(push_loop(self, loop_start));

  ASSERT_NOT_ERR(patch_jump(self, jump_start));

  up_scope(self);

  return COMP_OK;
}

comp_result compile_exp_whl(gab_compiler *self, boolean assignable) {
  u64 loop_start = self->mod->bytecode.size - 1;

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  ASSERT_NOT_ERR(expect_token(self, TOKEN_COLON));

  u64 jump = push_jump(self, OP_POP_JUMP_IF_FALSE);

  down_scope(self);

  ASSERT_NOT_ERR(compile_expressions(self, 1, NULL));

  up_scope(self);

  ASSERT_NOT_ERR(push_loop(self, loop_start));

  ASSERT_NOT_ERR(patch_jump(self, jump));

  push_byte(self, OP_PUSH_NULL);

  return COMP_OK;
}

comp_result compile_exp_rtn(gab_compiler *self, boolean assignable) {
  if (match_token(self, TOKEN_NEWLINE) || match_token(self, TOKEN_END)) {
    // This doesn't seem foolproof here.
    push_byte(self, OP_PUSH_NULL);
    ASSERT_NOT_ERR(push_return(self, (exp_list){.have = 1, .is_var = false}));
  } else {
    exp_list ret_value;

    ASSERT_NOT_ERR(compile_expressions(self, VAR_RET, &ret_value));

    ASSERT_NOT_ERR(push_return(self, ret_value));
  }

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
    NONE(),       // COLON
    INFIX(bin, BITWISE_AND, false),           // AMPERSAND
    INFIX(dot, PROPERTY, true),              // DOT
    INFIX(bin, TERM, false),                  // DOT_DOT
    NONE(),                            // EQUAL
    INFIX(bin, EQUALITY, false),              // EQUALEQUAL
    POSTFIX(type),                            // QUESTION
    PREFIX_POSTFIX(glb, asrt),                      // BANG
    INFIX(bin, COMPARISON, false),            // LESSER
    INFIX(bin, EQUALITY, false),              // LESSEREQUAL
    INFIX(bin, COMPARISON, false),            // GREATER
    INFIX(bin, EQUALITY, false),              // GREATEREQUAL
    INFIX(mth, CALL, true),                            // ARROW
    NONE(),                            // FATARROW
    INFIX(and, AND, false),                   // AND
    INFIX(or, OR, false),                     // OR
    PREFIX(una),                       // NOT
    PREFIX_INFIX(lst, idx, PROPERTY, false),  // LBRACE
    NONE(),                            // RBRACE
    PREFIX_INFIX(obj, obj_call, CALL, false), // LBRACK
    NONE(),                            // RBRACK
    PREFIX_INFIX(grp, call, CALL, false),     // LPAREN
    NONE(),                            // RPAREN
    PREFIX_INFIX(lmd, bin, BITWISE_OR, false),            // PIPE
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

comp_result compile(gab_compiler *self, s_u8_ref src, s_u8_ref name) {

  down_frame(self, name);

  gab_lexer_create(&self->lex, src);
  self->mod->source_lines = self->lex.source_lines;

  ASSERT_NOT_ERR(eat_token(self));

  // Add the std library
  initialize_local(self, add_local(self, s_u8_ref_create_cstr("__global__")));

  comp_result res = compile_block_body(self, 1, NULL);

  ASSERT_NOT_ERR(res);

  self->mod->ntop_level = peek_frame(self, 0)->deepest_local - 2;

  up_frame(self);

  push_byte(self, OP_RETURN_1);

  return COMP_OK;
}

gab_result *gab_engine_compile(gab_engine *eng, s_u8 *src, s_u8_ref name,
                               u8 flags) {

  gab_compiler *compiler = CREATE_STRUCT(gab_compiler);
  gab_compiler_create(compiler);

  gab_module *module = gab_module_create(name, src);

  gab_engine_add_module(eng, module);

  compiler->mod = module;

  // Deactive garbage collection during compilation
  comp_result status = compile(compiler, s_u8_ref_create_s_u8(src), name);

  if (status != COMP_OK) {
    gab_result *error = compiler->error;
    DESTROY_STRUCT(compiler);
    return error;
  }

  if (flags & GAB_FLAG_DUMP_BYTECODE) {
    gab_module_dump(module, name);
  }

  DESTROY_STRUCT(compiler);

  return gab_compile_success(eng, module);
}
