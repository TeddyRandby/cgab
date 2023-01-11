#include "include/compiler.h"
#include "include/char.h"
#include "include/core.h"
#include "include/engine.h"
#include "include/gab.h"
#include "include/module.h"
#include "include/object.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *gab_token_names[] = {
#define TOKEN(name) #name,
#include "include/token.h"
#undef TOKEN
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
  PREC_UNARY,       // ! - not
  PREC_CALL,        // ( {
  PREC_PROPERTY,    // . :
  PREC_PRIMARY
} gab_precedence;

/*
  Compile rules used for Pratt parsing of expressions.
*/

typedef i32 (*gab_compile_func)(gab_engine *, gab_bc *, gab_module *, boolean);

typedef struct gab_compile_rule gab_compile_rule;
struct gab_compile_rule {
  gab_compile_func prefix;
  gab_compile_func infix;
  gab_compile_func postfix;
  gab_precedence prec;
  boolean multi_line;
};

void gab_bc_create(gab_bc *self, s_i8 source) {
  self->scope_depth = 0;
  self->frame_count = 0;
  self->panic = false;

  memset(self->frames, 0, sizeof(self->frames));
  gab_lexer_create(&self->lex, source);
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

static void dump_compiler_error(gab_bc *bc, gab_status e, const char *help_fmt,
                                ...);

static boolean match_token(gab_bc *bc, gab_token tok);

static i32 eat_token(gab_bc *bc);

//------------------- Token Helpers -----------------------
// Can't return an error.
static inline boolean match_token(gab_bc *bc, gab_token tok) {
  return bc->current_token == tok;
}

static inline boolean match_terminator(gab_bc *bc) {
  return match_token(bc, TOKEN_END) || match_token(bc, TOKEN_ELSE) ||
         match_token(bc, TOKEN_UNTIL);
}

// Returns less than 0 if there was an error, greater than 0 otherwise.
static i32 eat_token(gab_bc *bc) {
  bc->previous_token = bc->current_token;
  bc->current_token = gab_lexer_next(&bc->lex);
  bc->line = bc->lex.current_row;

  if (match_token(bc, TOKEN_ERROR)) {
    eat_token(bc);
    dump_compiler_error(bc, bc->lex.status, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 match_and_eat_token(gab_bc *bc, gab_token tok) {
  if (!match_token(bc, tok))
    return COMP_TOKEN_NO_MATCH;

  return eat_token(bc);
}

static inline i32 expect_token(gab_bc *bc, gab_token tok) {
  if (!match_token(bc, tok)) {
    eat_token(bc);
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected %s",
                        gab_token_names[tok]);
    return COMP_ERR;
  }

  return eat_token(bc);
}

static inline u16 add_constant(gab_module *mod, gab_value value) {
  return gab_module_add_constant(mod, value);
}

static inline void push_op(gab_bc *bc, gab_module *mod, gab_opcode op) {
  gab_module_push_op(mod, op, bc->previous_token, bc->line,
                     bc->lex.previous_token_src);
}

static inline void push_pop(gab_bc *bc, gab_module *mod, u8 n) {
  gab_module_push_pop(mod, n, bc->previous_token, bc->line,
                      bc->lex.previous_token_src);
}

static inline void push_store(gab_bc *bc, gab_module *mod, u8 local) {
  gab_module_push_store_local(mod, local, bc->previous_token, bc->line,
                              bc->lex.previous_token_src);
}

static inline void push_load_local(gab_bc *bc, gab_module *mod, u8 local) {
  gab_module_push_load_local(mod, local, bc->previous_token, bc->line,
                             bc->lex.previous_token_src);
}

static inline void push_byte(gab_bc *bc, gab_module *mod, u8 data) {
  gab_module_push_byte(mod, data, bc->previous_token, bc->line,
                       bc->lex.previous_token_src);
}

static inline void push_short(gab_bc *bc, gab_module *mod, u16 data) {
  gab_module_push_short(mod, data, bc->previous_token, bc->line,
                        bc->lex.previous_token_src);
}

//--------------------- Local Variable Helpers --------------
static gab_bc_frame *peek_frame(gab_bc *bc, u32 depth) {
  return &bc->frames[bc->frame_count - depth - 1];
}

static void initialize_local(gab_bc *bc, u8 local) {
  peek_frame(bc, 0)->locals_depth[local] = bc->scope_depth;
}

/* Returns COMP_ERR if an error is encountered, and otherwise the offset of the
 * local.
 */
static i32 add_local(gab_bc *bc, s_i8 name, u8 flags) {
  gab_bc_frame *frame = peek_frame(bc, 0);
  if (frame->local_count == LOCAL_MAX) {
    dump_compiler_error(bc, GAB_TOO_MANY_LOCALS, "");
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
static i32 add_upvalue(gab_bc *bc, u32 depth, u8 index, u8 flags) {
  gab_bc_frame *frame = peek_frame(bc, depth);
  u16 count = frame->upv_count;

  // Don't pull redundant upvalues
  for (int i = 0; i < count; i++) {
    if (frame->upvs_index[i] == index && (frame->upvs_flag[i]) == flags) {
      return i;
    }
  }

  if (count == UPVALUE_MAX) {
    dump_compiler_error(bc, GAB_TOO_MANY_UPVALUES, "");
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
static i32 resolve_local(gab_bc *bc, s_i8 name, u32 depth) {
  for (i32 local = peek_frame(bc, depth)->local_count - 1; local >= 0;
       local--) {
    if (s_i8_match(name, peek_frame(bc, depth)->locals_name[local])) {
      if (peek_frame(bc, depth)->locals_depth[local] == -1) {
        dump_compiler_error(bc, GAB_REFERENCE_BEFORE_INITIALIZE, "");
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
static i32 resolve_upvalue(gab_bc *bc, s_i8 name, u32 depth) {
  // Base case, hopefully conversion doesn't cause issues
  if (depth >= bc->frame_count - 1) {
    return COMP_UPVALUE_NOT_FOUND;
  }

  i32 local = resolve_local(bc, name, depth + 1);
  if (local >= 0) {
    u8 flags = (peek_frame(bc, depth + 1)->locals_flag[local] |= FLAG_CAPTURED);
    return add_upvalue(bc, depth, local, flags | FLAG_LOCAL);
  }

  i32 upvalue = resolve_upvalue(bc, name, depth + 1);
  if (upvalue >= 0) {
    u8 flags = peek_frame(bc, depth + 1)->upvs_flag[upvalue];
    return add_upvalue(bc, depth, upvalue, flags & ~FLAG_LOCAL);
  }

  return COMP_UPVALUE_NOT_FOUND;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_RESOLVED_TO_LOCAL if the id was a local, and
 * COMP_RESOLVED_TO_UPVALUE if the id was an upvalue.
 */
static i32 resolve_id(gab_bc *bc, s_i8 name, u8 *value_in) {

  i32 arg = resolve_local(bc, name, 0);

  if (arg == COMP_ERR)
    return arg;

  if (arg == COMP_LOCAL_NOT_FOUND) {

    arg = resolve_upvalue(bc, name, 0);
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

static void down_scope(gab_bc *bc) { bc->scope_depth++; }

static void up_scope(gab_bc *bc, gab_module *mod) {
  bc->scope_depth--;

  gab_bc_frame *frame = peek_frame(bc, 0);

  boolean capture = false;
  while (frame->local_count > 1 &&
         frame->locals_depth[frame->local_count - 1] > bc->scope_depth) {
    capture |= frame->locals_flag[frame->local_count - 1] & FLAG_CAPTURED;
    frame->local_count--;
  }

  // If this block contained a captured local, escape it.
  if (capture) {
    push_op(bc, mod, OP_CLOSE_UPVALUE);
    push_byte(bc, mod, frame->local_count);
  }
}

static void down_frame(gab_bc *bc, s_i8 name) {
  bc->frame_count++;
  // The first local in any given frame is reserved for the function being
  // called. It is immutable.
  peek_frame(bc, 0)->local_count = 0;
  peek_frame(bc, 0)->upv_count = 0;
  peek_frame(bc, 0)->deepest_local = 0;
  peek_frame(bc, 0)->function_name = name;

  u8 receiver = add_local(bc, s_i8_cstr("self"), 0);
  initialize_local(bc, receiver);
}

static void up_frame(gab_bc *bc) {
  peek_frame(bc, 0)->local_count = 0;
  peek_frame(bc, 0)->upv_count = 0;
  bc->frame_count--;
  bc->scope_depth--;
}

// Forward declare some functions
gab_compile_rule get_rule(gab_token k);
i32 compile_exp_prec(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     gab_precedence prec);
i32 compile_expression(gab_engine *gab, gab_bc *bc, gab_module *mod);
i32 compile_tuple(gab_engine *gab, gab_bc *bc, gab_module *mod, u8 want,
                  boolean *vse_out);

//---------------- Compiling Helpers -------------------
/*
  These functions consume tokens and can emit bytes.
*/

/* Returns COMP_ERR if an error was encountered, and otherwise the offset of the
 * local.
 */
static i32 compile_local(gab_bc *bc, s_i8 name, u8 flags) {

  for (i32 local = peek_frame(bc, 0)->local_count - 1; local >= 0; local--) {
    if (peek_frame(bc, 0)->locals_depth[local] != -1 &&
        peek_frame(bc, 0)->locals_depth[local] < bc->scope_depth) {
      break;
    }

    if (s_i8_match(name, peek_frame(bc, 0)->locals_name[local])) {
      dump_compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }
  }

  return add_local(bc, name, flags);
}

/* Returns COMP_ERR if an error was encountered, and otherwise COMP_OK
 */
i32 compile_parameters(gab_bc *bc) {
  if (!match_and_eat_token(bc, TOKEN_LPAREN)) {
    // Assumed to have zero arguments
    return 0;
  }

  if (match_and_eat_token(bc, TOKEN_RPAREN)) {
    return 0;
  }

  i32 result = 0;
  u8 narguments = 0;

  do {

    if (narguments == FARG_MAX) {
      dump_compiler_error(bc, GAB_TOO_MANY_PARAMETERS, "");
      return COMP_ERR;
    }

    narguments++;

    if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = bc->lex.previous_token_src;
    i32 local = compile_local(bc, name, 0);
    if (local < 0)
      return COMP_ERR;

    // Function arguments are "initialized"
    // here as well.
    initialize_local(bc, local);

  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return narguments;
}

static inline i32 skip_newlines(gab_bc *bc) {
  while (match_token(bc, TOKEN_NEWLINE)) {
    if (eat_token(bc) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

static inline i32 optional_newline(gab_bc *bc) {
  match_and_eat_token(bc, TOKEN_NEWLINE);
  return COMP_OK;
}

i32 compile_block_expression(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_block_body(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    dump_compiler_error(bc, GAB_MISSING_END, "");
    return COMP_ERR;
  }

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    dump_compiler_error(bc, GAB_MISSING_END, "");
    return COMP_ERR;
  }

  i32 result = compile_block_expression(gab, bc, mod);

  if (result < 0)
    return COMP_ERR;

  while (!match_terminator(bc) && !match_token(bc, TOKEN_EOF)) {
    push_pop(bc, mod, 1);

    result = compile_block_expression(gab, bc, mod);
    if (result < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_block(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  down_scope(bc);

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  if (compile_block_body(gab, bc, mod) < 0)
    return COMP_ERR;

  up_scope(bc, mod);

  if (match_token(bc, TOKEN_EOF)) {
    eat_token(bc);
    dump_compiler_error(bc, GAB_MISSING_END, "");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_function_body(gab_engine *gab, gab_bc *bc, gab_module *mod,
                          gab_obj_prototype *p, u64 skip_jump, u8 narguments) {
  // Start compiling the function's bytecode.
  // You can't just store a pointer to the beginning of the code
  // Because if the vector is EVER resized,
  // Then all your functions are suddenly invalid.
  u64 offset = mod->bytecode.len;

  i32 result = compile_block(gab, bc, mod);

  if (result < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  gab_module_push_return(mod, result, false, bc->previous_token, bc->line,
                         bc->lex.previous_token_src);

  gab_module_patch_jump(mod, skip_jump);

  gab_bc_frame *frame = peek_frame(bc, 0);

  u8 nupvalues = frame->upv_count;

  u8 nlocals = frame->deepest_local - narguments - 1;

  u64 len = mod->bytecode.len - offset;

  p->offset = offset;
  p->len = len;
  p->nupvalues = nupvalues;
  p->nlocals = nlocals;
  p->narguments = narguments;

  return COMP_OK;
}

i32 compile_function_specialization(gab_engine *gab, gab_bc *bc,
                                    gab_module *mod, u16 func_constant,
                                    s_i8 name) {
  if (match_and_eat_token(bc, TOKEN_LBRACE)) {
    if (match_token(bc, TOKEN_RBRACE)) {
      push_op(bc, mod, OP_PUSH_NULL);
    } else {
      if (compile_expression(gab, bc, mod) < 0)
        return COMP_ERR;
    }

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;
  } else {
    // An implicit specialization
    // Push the implicit receiver anytype
    push_op(bc, mod, OP_PUSH_TYPEANY);
  }

  gab_obj_prototype *p = gab_obj_prototype_create(mod);

  u64 skip_jump = gab_module_push_jump(mod, OP_JUMP, bc->previous_token,
                                       bc->line, bc->lex.previous_token_src);

  // Doesnt need a pop scope because the scope is popped with the callframe.
  down_scope(bc);

  down_frame(bc, name);

  i32 narguments = compile_parameters(bc);

  if (narguments < 0)
    return COMP_ERR;

  if (compile_function_body(gab, bc, mod, p, skip_jump, narguments) < 0)
    return COMP_ERR;

  u16 proto_constant = add_constant(mod, GAB_VAL_OBJ(p));

  // Create the closure, adding a specialization to the pushed function.
  push_op(bc, mod, OP_CLOSURE);
  push_short(bc, mod, proto_constant);
  push_short(bc, mod, func_constant);

  gab_bc_frame *frame = peek_frame(bc, 0);
  for (int i = 0; i < p->nupvalues; i++) {
    push_byte(bc, mod, frame->upvs_flag[i]);
    push_byte(bc, mod, frame->upvs_index[i]);
  }

  up_frame(bc);
  return COMP_OK;
}

i32 compile_expression(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  return compile_exp_prec(gab, bc, mod, PREC_ASSIGNMENT);
}

i32 compile_tuple(gab_engine *gab, gab_bc *bc, gab_module *mod, u8 want,
                  boolean *vse_out) {
  u8 have = 0;
  boolean vse;

  i32 result;
  do {
    if (optional_newline(bc) < 0)
      return COMP_ERR;

    vse = false;
    result = compile_exp_prec(gab, bc, mod, PREC_ASSIGNMENT);

    if (result < 0)
      return COMP_ERR;

    vse = result == VAR_RET;
    have++;
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)) > 0);

  if (result < 0)
    return COMP_ERR;

  if (have > want) {
    dump_compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS, "");
    return COMP_ERR;
  }

  if (vse) {
    // If our expression list is variable length

    if (want == VAR_RET) {
      // Here, take all expressions from the VSE.
      // Have no represents the additional expressions, so decrement it once.
      have--;
      gab_module_try_patch_vse(mod, VAR_RET);
    } else {
      // Here, only take enough to fill out the remaining.
      gab_module_try_patch_vse(mod, want - have + 1);
      // We now have all the expressions we want
      have = want;
    }

  } else {
    // If our expression list is constant length
    if (want != VAR_RET) {
      // If we want a constant number of expressions
      while (have < want) {
        // While we have fewer expressions than we want, push nulls.
        push_op(bc, mod, OP_PUSH_NULL);
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

i32 add_function_constant(gab_engine *gab, gab_module *mod, s_i8 name) {
  gab_obj_function *f = gab_obj_function_create(gab, name);

  return add_constant(mod, GAB_VAL_OBJ(f));
}

i32 add_string_constant(gab_engine *gab, gab_module *mod, s_i8 str) {
  gab_obj_string *obj = gab_obj_string_create(gab, str);

  return add_constant(mod, GAB_VAL_OBJ(obj));
}

i32 compile_id_constant(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  return add_string_constant(gab, mod, bc->lex.previous_token_src);
}

i32 compile_property(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     boolean assignable) {
  i32 prop = compile_id_constant(gab, bc, mod);
  if (prop < 0)
    return COMP_ERR;

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
      return COMP_ERR;
    }

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    push_op(bc, mod, OP_STORE_PROPERTY_ANA);
    push_short(bc, mod, prop);

    // The cache'd offset
    push_short(bc, mod, 0);
    // The shape at this get
    gab_module_push_inline_cache(mod, bc->previous_token, bc->line,
                                 bc->lex.previous_token_src);

    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_op(bc, mod, OP_LOAD_PROPERTY_ANA);
    push_short(bc, mod, prop);

    // The cache'd offset
    push_short(bc, mod, 0);
    // The shape at this get
    gab_module_push_inline_cache(mod, bc->previous_token, bc->line,
                                 bc->lex.previous_token_src);
    break;
  }

  default:
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling property access");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_lst_internal_item(gab_engine *gab, gab_bc *bc, gab_module *mod,
                              u8 index) {

  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_lst_internals(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  u8 size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (!match_token(bc, TOKEN_RBRACE)) {

    if (compile_lst_internal_item(gab, bc, mod, size) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      dump_compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(bc) < 0)
      return COMP_ERR;
    size++;
  }

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  return size;
}

// Forward decl
i32 compile_definition(gab_engine *gab, gab_bc *bc, gab_module *mod, s_i8 name);

i32 compile_obj_internal_item(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  if (match_and_eat_token(bc, TOKEN_IDENTIFIER)) {
    // Get the string for the key and push the key.
    s_i8 name = bc->lex.previous_token_src;

    gab_obj_string *obj =
        gab_obj_string_create(gab, bc->lex.previous_token_src);

    // Push the constant onto the stack.
    push_op(bc, mod, OP_CONSTANT);
    push_short(bc, mod, add_constant(mod, GAB_VAL_OBJ(obj)));

    // Compile the expression if theres a colon, or look for a local with
    // the name and use that as the value.
    switch (match_and_eat_token(bc, TOKEN_COLON)) {

    case COMP_OK: {
      if (compile_expression(gab, bc, mod) < 0)
        return COMP_ERR;

      return COMP_OK;
    }

    case COMP_TOKEN_NO_MATCH: {
      u8 value_in;

      i32 result = resolve_id(bc, name, &value_in);

      switch (result) {

      case COMP_RESOLVED_TO_LOCAL:
        push_load_local(bc, mod, value_in);
        break;

      case COMP_RESOLVED_TO_UPVALUE:
        gab_module_push_load_upvalue(mod, value_in, bc->previous_token,
                                     bc->line, bc->lex.previous_token_src,
                                     peek_frame(bc, 0)->upvs_flag[value_in] &
                                         FLAG_MUTABLE);
        break;

      case COMP_ID_NOT_FOUND:
        push_op(bc, mod, OP_PUSH_TRUE);
        break;

      default:
        dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                            "While compiling object literal");
        return COMP_ERR;
      }

      return COMP_OK;
    }

    default:
      goto fin;
    }
  }

  if (match_and_eat_token(bc, TOKEN_LBRACE)) {

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_RBRACE) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_COLON) < 0)
      return COMP_ERR;

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    return COMP_OK;
  }

fin:
  eat_token(bc);
  dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                      "While compiling object literal");
  return COMP_ERR;
}

i32 compile_obj_internals(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  u8 size = 0;

  if (skip_newlines(bc) < 0)
    return COMP_ERR;

  while (match_and_eat_token(bc, TOKEN_RBRACK) == COMP_TOKEN_NO_MATCH) {

    if (compile_obj_internal_item(gab, bc, mod) < 0)
      return COMP_ERR;

    if (size == UINT8_MAX) {
      dump_compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_INITIALIZER, "");
      return COMP_ERR;
    }

    if (skip_newlines(bc) < 0)
      return COMP_ERR;

    size++;
  };

  return size;
}

i32 compile_record(gab_engine *gab, gab_bc *bc, gab_module *mod, s_i8 name) {
  boolean is_def = name.data != NULL;
  i32 name_const;

  if (is_def) {
    name_const = add_string_constant(gab, mod, name);
  }

  i32 size = compile_obj_internals(gab, bc, mod);

  if (size < 0)
    return COMP_ERR;

  push_op(bc, mod, is_def ? OP_OBJECT_RECORD_DEF : OP_OBJECT_RECORD);

  if (is_def)
    push_short(bc, mod, name_const);

  push_byte(bc, mod, size);
  return COMP_OK;
}

i32 compile_array(gab_engine *gab, gab_bc *bc, gab_module *mod) {
  i32 size = compile_lst_internals(gab, bc, mod);

  if (size < 0)
    return COMP_ERR;

  push_op(bc, mod, OP_OBJECT_ARRAY);
  push_byte(bc, mod, size);
  return COMP_OK;
}

i32 compile_definition(gab_engine *gab, gab_bc *bc, gab_module *mod,
                       s_i8 name) {

  // A record definition
  if (match_and_eat_token(bc, TOKEN_LBRACK)) {
    u8 local = add_local(bc, name, 0);
    initialize_local(bc, local);

    if (compile_record(gab, bc, mod, name) < 0)
      return COMP_ERR;

    push_op(bc, mod, OP_TYPE);

    gab_module_push_store_local(mod, local, bc->previous_token, bc->line,
                                bc->lex.previous_token_src);

    return COMP_OK;
  }

  // Const variable
  if (match_and_eat_token(bc, TOKEN_EQUAL)) {
    u8 local = add_local(bc, name, 0);

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    initialize_local(bc, local);

    gab_module_push_store_local(mod, local, bc->previous_token, bc->line,
                                bc->lex.previous_token_src);

    return COMP_OK;
  }

  // From now on, we know its a function definition.

  // A specialization

  gab_obj_function *f = gab_obj_function_create(gab, name);
  u16 func_constant = add_constant(mod, GAB_VAL_OBJ(f));

  // Create a local to store the new function in
  u8 local = add_local(bc, name, 0);
  initialize_local(bc, local);

  // Now compile the specialization
  if (compile_function_specialization(gab, bc, mod, func_constant, name) < 0)
    return COMP_ERR;

  gab_module_push_store_local(mod, local, bc->previous_token, bc->line,
                              bc->lex.previous_token_src);
  return COMP_OK;
}

//---------------- Compiling Expressions ------------------

i32 compile_exp_blk(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  static u64 anonymous_count = 0;
  static i8 name_buff[25];

  if (match_token(bc, TOKEN_LPAREN) || match_token(bc, TOKEN_LBRACE)) {
    i32 len =
        snprintf((char *)name_buff + 0, 25, "anonymous_%lu", anonymous_count);

    s_i8 name = s_i8_create(name_buff + 0, len);

    gab_obj_function *f = gab_obj_function_create(gab, name);
    u16 f_constant = add_constant(mod, GAB_VAL_OBJ(f));

    // We are an anonyumous function
    if (compile_function_specialization(gab, bc, mod, f_constant, name) < 0)
      return COMP_ERR;
  } else {
    if (compile_block(gab, bc, mod) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_if(gab_engine *gab, gab_bc *bc, gab_module *mod,
                   boolean assignable) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  u64 then_jump =
      gab_module_push_jump(mod, OP_POP_JUMP_IF_FALSE, bc->previous_token,
                           bc->line, bc->lex.previous_token_src);

  i32 result = compile_block(gab, bc, mod);
  if (result < 0)
    return COMP_ERR;

  u64 else_jump = gab_module_push_jump(mod, OP_JUMP, bc->previous_token,
                                       bc->line, bc->lex.previous_token_src);

  gab_module_patch_jump(mod, then_jump);

  switch (match_and_eat_token(bc, TOKEN_ELSE)) {
  case COMP_OK:
    if (compile_block(gab, bc, mod) < 0)
      return COMP_ERR;

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    break;

  case COMP_TOKEN_NO_MATCH:
    for (u8 i = 0; i < result; i++)
      push_op(bc, mod, OP_PUSH_NULL);

    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;
    break;

  default:
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling if expression");
    return COMP_ERR;
  }

  gab_module_patch_jump(mod, else_jump);

  return COMP_OK;
}

i32 compile_exp_mch(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_NEWLINE) < 0)
    return COMP_ERR;

  u64 next = 0;

  v_u64 done_jumps;
  v_u64_create(&done_jumps, 8);

  // While we don't match the closing question
  while (match_and_eat_token(bc, TOKEN_QUESTION) == COMP_TOKEN_NO_MATCH) {
    if (next != 0)
      gab_module_patch_jump(mod, next);

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    push_op(bc, mod, OP_MATCH);

    next = gab_module_push_jump(mod, OP_POP_JUMP_IF_FALSE, bc->previous_token,
                                bc->line, bc->lex.previous_token_src);

    if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
      return COMP_ERR;

    if (optional_newline(bc) < 0)
      return COMP_ERR;

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    // Push a jump out of the match statement at the end of every case.
    v_u64_push(&done_jumps,
               gab_module_push_jump(mod, OP_JUMP, bc->previous_token, bc->line,
                                    bc->lex.previous_token_src));

    if (expect_token(bc, TOKEN_NEWLINE) < 0)
      return COMP_ERR;
  }

  // If none of the cases match, the last jump should end up here.
  gab_module_patch_jump(mod, next);

  // Pop the pattern that we never matched
  push_pop(bc, mod, 1);

  if (expect_token(bc, TOKEN_FAT_ARROW) < 0)
    return COMP_ERR;

  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  for (i32 i = 0; i < done_jumps.len; i++) {
    // Patch all the jumps to the end of the match expression.
    gab_module_patch_jump(mod, v_u64_val_at(&done_jumps, i));
  }

  v_u64_destroy(&done_jumps);

  return COMP_OK;
}

/*
 * Postfix assert expression.
 */
i32 compile_exp_asrt(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     boolean assignable) {
  push_op(bc, mod, OP_ASSERT);
  return COMP_OK;
}

/*
 * Postfix type expression.
 */
i32 compile_exp_type(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     boolean assignable) {
  push_op(bc, mod, OP_TYPE);
  return COMP_OK;
}

/*
 * Infix is expression.
 */
i32 compile_exp_is(gab_engine *gab, gab_bc *bc, gab_module *mod,
                   boolean assignable) {
  push_op(bc, mod, OP_TYPE);

  if (compile_exp_prec(gab, bc, mod, PREC_EQUALITY) < 0)
    return COMP_ERR;

  push_op(bc, mod, OP_EQUAL);

  return COMP_OK;
}

/*
 * Return COMP_ERR if an error occurs, and the size of the expression otherwise.
 */
i32 compile_exp_bin(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  gab_token op = bc->previous_token;

  i32 result = compile_exp_prec(gab, bc, mod, get_rule(op).prec + 1);

  if (result < 0)
    return COMP_ERR;

  switch (op) {
  case TOKEN_MINUS: {
    push_op(bc, mod, OP_SUBTRACT);
    break;
  }
  case TOKEN_PLUS: {
    push_op(bc, mod, OP_ADD);
    break;
  }
  case TOKEN_STAR: {
    push_op(bc, mod, OP_MULTIPLY);
    break;
  }
  case TOKEN_SLASH: {
    push_op(bc, mod, OP_DIVIDE);
    break;
  }
  case TOKEN_PERCENT: {
    push_op(bc, mod, OP_MODULO);
    break;
  }
  case TOKEN_DOT_DOT: {
    push_op(bc, mod, OP_CONCAT);
    break;
  }
  case TOKEN_EQUAL_EQUAL: {
    push_op(bc, mod, OP_EQUAL);
    break;
  }
  case TOKEN_LESSER: {
    push_op(bc, mod, OP_LESSER);
    break;
  }
  case TOKEN_LESSER_EQUAL: {
    push_op(bc, mod, OP_LESSER_EQUAL);
    break;
  }
  case TOKEN_GREATER: {
    push_op(bc, mod, OP_GREATER);
    break;
  }
  case TOKEN_GREATER_EQUAL: {
    push_op(bc, mod, OP_GREATER_EQUAL);
    break;
  }
  default: {
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling binary expression");
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

i32 compile_exp_una(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  gab_token op = bc->previous_token;

  i32 result = compile_exp_prec(gab, bc, mod, PREC_UNARY);

  if (result < 0)
    return COMP_ERR;

  switch (op) {
  case TOKEN_MINUS: {
    push_op(bc, mod, OP_NEGATE);
    break;
  }
  case TOKEN_NOT: {
    push_op(bc, mod, OP_NOT);
    break;
  }
  default: {
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling unary expression");
    return COMP_ERR;
  }
  }

  return COMP_OK;
}

/*
 * Returns null if an error occured.
 */
a_i8 *parse_raw_str(s_i8 raw_str) {
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
i32 compile_exp_str(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  s_i8 raw_token = bc->lex.previous_token_src;

  a_i8 *parsed = parse_raw_str(raw_token);

  if (parsed == NULL) {
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling string literal");
    return COMP_ERR;
  }

  gab_obj_string *obj =
      gab_obj_string_create(gab, s_i8_create(parsed->data, parsed->len));

  a_i8_destroy(parsed);

  push_op(bc, mod, OP_CONSTANT);
  push_short(bc, mod, add_constant(mod, GAB_VAL_OBJ(obj)));

  return COMP_OK;
}

/*
 * Returns COMP_ERR if an error occured, otherwise the size of the expressions
 */
i32 compile_exp_itp(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_exp_str(gab, bc, mod, assignable) < 0)
    return COMP_ERR;

  if (match_token(bc, TOKEN_INTERPOLATION_END)) {
    // Empty interpolation
    goto fin;
  }

  if (!match_token(bc, TOKEN_INTERPOLATION)) {
    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    push_op(bc, mod, OP_STRINGIFY);
    push_op(bc, mod, OP_CONCAT);
  }

  i32 result;
  while ((result = match_and_eat_token(bc, TOKEN_INTERPOLATION))) {

    if (compile_exp_str(gab, bc, mod, assignable) < 0)
      return COMP_ERR;

    if (match_token(bc, TOKEN_INTERPOLATION_END)) {
      goto fin;
    }

    if (!match_token(bc, TOKEN_INTERPOLATION)) {

      if (compile_expression(gab, bc, mod) < 0)
        return COMP_ERR;

      push_op(bc, mod, OP_STRINGIFY);
      push_op(bc, mod, OP_CONCAT);
    }

    // concat this to the long-running string.
    push_op(bc, mod, OP_CONCAT);
  }

  if (result < 0)
    return COMP_ERR;

fin:
  if (expect_token(bc, TOKEN_INTERPOLATION_END) < 0)
    return COMP_ERR;

  if (compile_exp_str(gab, bc, mod, assignable) < 0)
    return COMP_ERR;

  // Concat the final string.
  push_op(bc, mod, OP_CONCAT);
  return COMP_OK;
}

i32 compile_exp_grp(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_num(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  f64 num = strtod((char *)bc->lex.previous_token_src.data, NULL);
  push_op(bc, mod, OP_CONSTANT);
  push_short(bc, mod, add_constant(mod, GAB_VAL_NUMBER(num)));

  return COMP_OK;
}

i32 compile_exp_bool(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     boolean assignable) {
  push_byte(bc, mod,
            bc->previous_token == TOKEN_TRUE ? OP_PUSH_TRUE : OP_PUSH_FALSE);
  return COMP_OK;
}

i32 compile_exp_null(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     boolean assignable) {
  push_op(bc, mod, OP_PUSH_NULL);
  return COMP_OK;
}

i32 compile_exp_def(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  s_i8 name = bc->lex.previous_token_src;

  if (compile_definition(gab, bc, mod, name) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_exp_arr(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  return compile_array(gab, bc, mod);
}

i32 compile_exp_spd(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  push_op(bc, mod, OP_SPREAD);
  // Want byte
  push_byte(bc, mod, 1);

  return VAR_RET;
}

i32 compile_exp_rec(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  return compile_record(gab, bc, mod, (s_i8){0});
}

i32 compile_exp_let(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  u8 locals[16] = {0};

  u8 local_count = 0;

  i32 result;
  do {
    if (local_count == 16) {
      dump_compiler_error(bc, GAB_TOO_MANY_EXPRESSIONS_IN_LET, "");
      return COMP_ERR;
    }

    if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = bc->lex.previous_token_src;

    i32 result = resolve_id(bc, bc->lex.previous_token_src, NULL);

    switch (result) {

    case COMP_ID_NOT_FOUND: {
      i32 loc = compile_local(bc, name, FLAG_MUTABLE);

      if (loc < 0)
        return COMP_ERR;

      locals[local_count] = loc;
      break;
    }

    case COMP_RESOLVED_TO_LOCAL:
    case COMP_RESOLVED_TO_UPVALUE: {
      dump_compiler_error(bc, GAB_LOCAL_ALREADY_EXISTS, "");
      return COMP_ERR;
    }

    default:
      dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                          "While compiling let expression");
      return COMP_ERR;
    }

    local_count++;

  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (compile_tuple(gab, bc, mod, local_count, NULL) < 0)
      return COMP_ERR;
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    dump_compiler_error(bc, GAB_MISSING_INITIALIZER, "");
    return COMP_ERR;

  default:
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling 'let' expression");
    return COMP_ERR;
  }

  while (local_count--) {
    u8 local = locals[local_count];
    initialize_local(bc, local);

    if (local_count > 0) {
      gab_module_push_store_local(mod, local, bc->previous_token, bc->line,
                                  bc->lex.previous_token_src);
      push_pop(bc, mod, 1);
    } else {
      gab_module_push_store_local(mod, local, bc->previous_token, bc->line,
                                  bc->lex.previous_token_src);
    }
  }

  return COMP_OK;
}

i32 compile_exp_idn(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  s_i8 name = bc->lex.previous_token_src;

  u8 var;
  boolean is_local_var = true;

  switch (resolve_id(bc, name, &var)) {
  case COMP_RESOLVED_TO_LOCAL: {
    break;
  }

  case COMP_RESOLVED_TO_UPVALUE: {
    is_local_var = false;
    break;
  }

  case COMP_ID_NOT_FOUND: {
    dump_compiler_error(bc, GAB_MISSING_IDENTIFIER, "");
    return COMP_ERR;
  }

  default:
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN, "While compiling identifier");
    return COMP_ERR;
  }

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (!assignable) {
      dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                          "While compiling identifier");
      return COMP_ERR;
    }

    gab_bc_frame *frame = peek_frame(bc, 0);
    if (is_local_var) {
      if (!(frame->locals_flag[var] & FLAG_MUTABLE)) {
        s_i8 name = frame->locals_name[var];
        dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                            "Variable '%.*s' is not mutable", (i32)name.len,
                            name.data);
        return COMP_ERR;
      }
    } else {
      if (!(frame->upvs_flag[var] & FLAG_MUTABLE)) {
        s_i8 name = frame->locals_name[var];
        dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                            "Variable '%.*s' is not mutable", (i32)name.len,
                            name.data);
        return COMP_ERR;
      }
    }

    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    if (is_local_var) {
      gab_module_push_store_local(mod, var, bc->previous_token, bc->line,
                                  bc->lex.previous_token_src);
    } else {
      gab_module_push_store_upvalue(mod, var, bc->previous_token, bc->line,
                                    bc->lex.previous_token_src);
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH:
    if (is_local_var) {
      push_load_local(bc, mod, var);
    } else {
      gab_module_push_load_upvalue(
          mod, var, bc->previous_token, bc->line, bc->lex.previous_token_src,
          peek_frame(bc, 0)->upvs_flag[var] & FLAG_MUTABLE);
    }
    break;

  default:
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN,
                        "While compiling 'identifier' expression");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_idx(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_RBRACE) < 0)
    return COMP_ERR;

  switch (match_and_eat_token(bc, TOKEN_EQUAL)) {

  case COMP_OK: {
    if (assignable) {
      if (compile_expression(gab, bc, mod) < 0)
        return COMP_ERR;

      push_op(bc, mod, OP_STORE_INDEX);
    } else {
      dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                          "While compiling 'index' expression");
      return COMP_ERR;
    }
    break;
  }

  case COMP_TOKEN_NO_MATCH: {
    push_op(bc, mod, OP_LOAD_INDEX);
    break;
  }

  default:
    dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE,
                        "While compiling 'index' expression");
    return COMP_ERR;
  }

  return COMP_OK;
}

i32 compile_exp_dot(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_property(gab, bc, mod, assignable) < 0)
    return COMP_ERR;

  return COMP_OK;
}

i32 compile_arg_list(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     boolean *vse_out) {
  i32 nargs = 0;

  if (!match_token(bc, TOKEN_RPAREN)) {

    nargs = compile_tuple(gab, bc, mod, VAR_RET, vse_out);

    if (nargs < 0)
      return COMP_ERR;
  }

  if (expect_token(bc, TOKEN_RPAREN) < 0)
    return COMP_ERR;

  return nargs;
}

i32 compile_arguments(gab_engine *gab, gab_bc *bc, gab_module *mod,
                      boolean *vse_out) {

  if (match_and_eat_token(bc, TOKEN_LPAREN)) {
    // Normal function args
    return compile_arg_list(gab, bc, mod, vse_out);
  }

  if (match_and_eat_token(bc, TOKEN_LBRACE)) {
    // record argument
    if (compile_record(gab, bc, mod, (s_i8){0}) < 0)
      return COMP_ERR;

    return 1;
  }

  return 0;
}

i32 compile_exp_mth(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {

  s_i8 message = bc->lex.previous_token_src;
  // SKip the ':' at the beginning
  message.data++;
  message.len--;

  u16 f = add_function_constant(gab, mod, message);

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, mod, &vse);

  if (result > 16) {
    dump_compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_call(mod, result, vse, f, bc->previous_token, bc->line,
                       bc->lex.previous_token_src);

  return VAR_RET;
}

i32 compile_exp_cal(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  push_op(bc, mod, OP_PUSH_NULL);
  push_op(bc, mod, OP_SWAP);

  boolean vse = false;
  i32 result = compile_arg_list(gab, bc, mod, &vse);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    dump_compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_dyncall(mod, result, vse, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);

  return VAR_RET;
}

i32 compile_exp_vam(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, mod, &vse);

  if (result > 16) {
    dump_compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_dyncall(mod, result, vse, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);

  return VAR_RET;
}

i32 compile_exp_empty_vam(gab_engine *gab, gab_bc *bc, gab_module *mod,
                          boolean assignable) {
  push_op(bc, mod, OP_PUSH_NULL);
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  boolean vse = false;
  i32 result = compile_arguments(gab, bc, mod, &vse);

  if (result > 16) {
    dump_compiler_error(bc, GAB_TOO_MANY_ARGUMENTS, "");
    return COMP_ERR;
  }

  gab_module_push_dyncall(mod, result, vse, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);

  return VAR_RET;
}

i32 compile_exp_empty_mth(gab_engine *gab, gab_bc *bc, gab_module *mod,
                          boolean assignable) {
  push_op(bc, mod, OP_PUSH_NULL);
  return compile_exp_mth(gab, bc, mod, assignable);
}

i32 compile_exp_and(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  u64 end_jump = gab_module_push_jump(mod, OP_LOGICAL_AND, bc->previous_token,
                                      bc->line, bc->lex.previous_token_src);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, mod, PREC_AND) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod, end_jump);

  return COMP_OK;
}

i32 compile_exp_or(gab_engine *gab, gab_bc *bc, gab_module *mod,
                   boolean assignable) {
  u64 end_jump = gab_module_push_jump(mod, OP_LOGICAL_OR, bc->previous_token,
                                      bc->line, bc->lex.previous_token_src);

  if (optional_newline(bc) < 0)
    return COMP_ERR;

  if (compile_exp_prec(gab, bc, mod, PREC_OR) < 0)
    return COMP_ERR;

  gab_module_patch_jump(mod, end_jump);

  return COMP_OK;
}

i32 compile_exp_prec(gab_engine *gab, gab_bc *bc, gab_module *mod,
                     gab_precedence prec) {

  if (eat_token(bc) < 0)
    return COMP_ERR;

  gab_compile_rule rule = get_rule(bc->previous_token);

  if (rule.prefix == NULL) {
    dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN, "Expected an expression");
    return COMP_ERR;
  }

  boolean assignable = prec <= PREC_ASSIGNMENT;

  i32 have = rule.prefix(gab, bc, mod, assignable);
  if (have < 0)
    return COMP_ERR;

  while (prec <= get_rule(bc->current_token).prec) {
    if (have < 0)
      return COMP_ERR;

    if (eat_token(bc) < 0)
      return COMP_ERR;

    rule = get_rule(bc->previous_token);

    // Maybe try and see if the current token has a rule for prefix here?
    if (rule.infix == NULL) {
      // No infix rule for this token it might be a postfix though.

      if (rule.postfix == NULL) {
        dump_compiler_error(bc, GAB_UNEXPECTED_TOKEN, "");
        return COMP_ERR;
      }

      have = rule.postfix(gab, bc, mod, assignable);
    } else {
      // Treat this as an infix expression.
      have = rule.infix(gab, bc, mod, assignable);
    }
  }

  // TODO: See if this actually does anything.
  if (assignable && match_token(bc, TOKEN_EQUAL)) {
    dump_compiler_error(bc, GAB_EXPRESSION_NOT_ASSIGNABLE, "");
    return COMP_ERR;
  }

  return have;
}

/*
 * For loops are broken
 */
i32 compile_exp_for(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  down_scope(bc);

  u8 local_start = peek_frame(bc, 0)->local_count - 1;
  u16 loop_locals = 0;
  i32 result;

  do {
    if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
      return COMP_ERR;

    s_i8 name = bc->lex.previous_token_src;

    i32 loc = compile_local(bc, name, 0);

    if (loc < 0)
      return COMP_ERR;

    initialize_local(bc, loc);
    loop_locals++;
  } while ((result = match_and_eat_token(bc, TOKEN_COMMA)));

  if (result == COMP_ERR)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_IN) < 0)
    return COMP_ERR;

  // This is the iterator function
  if (compile_expression(gab, bc, mod) < 0)
    return COMP_ERR;

  u64 loop = gab_module_push_loop(mod);

  // Load the funciton.
  push_op(bc, mod, OP_DUP);

  // Call the function, wanting loop_locals results.
  push_op(bc, mod, OP_CALL_0);
  push_byte(bc, mod, loop_locals);

  // Exit the for loop if the last loop local is false.
  u64 jump_start =
      gab_module_push_jump(mod, OP_JUMP_IF_FALSE, bc->previous_token, bc->line,
                           bc->lex.previous_token_src);
  // Pop the results in reverse order, assigning them to each loop local.
  for (u8 ll = 0; ll < loop_locals; ll++) {
    push_store(bc, mod, local_start + loop_locals - ll);
    push_pop(bc, mod, 1);
  }

  if (compile_block(gab, bc, mod) < 0)
    return COMP_ERR;

  if (expect_token(bc, TOKEN_END) < 0)
    return COMP_ERR;

  push_pop(bc, mod, 1);

  gab_module_patch_loop(mod, loop, bc->previous_token, bc->line,
                        bc->lex.previous_token_src);

  gab_module_patch_jump(mod, jump_start);

  up_scope(bc, mod);

  push_pop(bc, mod, 1);

  return COMP_OK;
}

i32 compile_exp_lop(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  down_scope(bc);

  u64 loop = gab_module_push_loop(mod);

  if (compile_block(gab, bc, mod) < 0)
    return COMP_ERR;

  if (match_and_eat_token(bc, TOKEN_UNTIL)) {
    if (compile_expression(gab, bc, mod) < 0)
      return COMP_ERR;

    u64 jump =
        gab_module_push_jump(mod, OP_POP_JUMP_IF_TRUE, bc->previous_token,
                             bc->line, bc->lex.previous_token_src);

    push_pop(bc, mod, 1);

    gab_module_patch_loop(mod, loop, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);

    gab_module_patch_jump(mod, jump);
  } else {
    if (expect_token(bc, TOKEN_END) < 0)
      return COMP_ERR;

    push_pop(bc, mod, 1);

    gab_module_patch_loop(mod, loop, bc->previous_token, bc->line,
                          bc->lex.previous_token_src);
  }

  push_op(bc, mod, OP_PUSH_NULL);

  up_scope(bc, mod);
  return COMP_OK;
}

i32 compile_exp_sym(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (expect_token(bc, TOKEN_IDENTIFIER) < 0)
    return COMP_ERR;

  gab_obj_symbol *sym = gab_obj_symbol_create(bc->lex.previous_token_src);

  push_op(bc, mod, OP_CONSTANT);
  push_short(bc, mod, add_constant(mod, GAB_VAL_OBJ(sym)));

  return COMP_OK;
}

i32 compile_exp_rtn(gab_engine *gab, gab_bc *bc, gab_module *mod,
                    boolean assignable) {
  if (match_token(bc, TOKEN_NEWLINE)) {
    push_op(bc, mod, OP_PUSH_NULL);
    push_op(bc, mod, OP_RETURN_1);
    return COMP_OK;
  }

  boolean vse;
  i32 result = compile_tuple(gab, bc, mod, VAR_RET, &vse);

  if (result < 0)
    return COMP_ERR;

  if (result > 16) {
    dump_compiler_error(bc, GAB_TOO_MANY_RETURN_VALUES, "");
    return COMP_ERR;
  }

  gab_module_push_return(mod, result, vse, bc->previous_token, bc->line,
                         bc->lex.previous_token_src);
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
    PREFIX(lop),                       // LOOP
    NONE(),                       // UNTIL
    INFIX(bin, TERM, false),                  // PLUS
    PREFIX_INFIX(una, bin, TERM, false),      // MINUS
    INFIX(bin, FACTOR, false),                // STAR
    INFIX(bin, FACTOR, false),                // SLASH
    INFIX(bin, FACTOR, false),                // PERCENT
    NONE(),                            // COMMA
    PREFIX_INFIX(empty_vam, vam, CALL, true),       // COLON
    NONE(),           // AMPERSAND
    PREFIX(sym),           // DOLLAR
    PREFIX_INFIX(empty_mth, mth, CALL, true), // MESSAGE
    INFIX(dot, PROPERTY, true),              // DOT
    PREFIX_INFIX(spd, bin, TERM, false),                  // DOT_DOT
    NONE(),                            // EQUAL
    INFIX(bin, EQUALITY, false),              // EQUALEQUAL
    POSTFIX(type),                            // QUESTION
    POSTFIX(asrt),                      // BANG
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
    PREFIX(rec), // LBRACK
    NONE(),                            // RBRACK
    PREFIX_INFIX(grp, cal, CALL, false),     // LPAREN
    NONE(),                            // RPAREN
    NONE(),            // PIPE
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

i32 compile(gab_engine *gab, gab_bc *bc, gab_module *mod, s_i8 name,
            u8 narguments, s_i8 arguments[narguments]) {
  down_frame(bc, name);

  if (eat_token(bc) == COMP_ERR)
    return -1;

  for (u8 i = 0; i < narguments; i++) {
    initialize_local(bc, add_local(bc, arguments[i], 0));
  }

  if (compile_block_body(gab, bc, mod) < 0)
    return -1;

  push_op(bc, mod, OP_RETURN_1);

  u8 top_level_locals = peek_frame(bc, 0)->deepest_local - narguments - 1;

  gab_obj_function *f = gab_obj_function_create(gab, name);

  gab_obj_prototype *p = gab_obj_prototype_create(mod);
  p->nupvalues = 0;
  p->narguments = narguments;
  p->nlocals = top_level_locals;
  p->offset = 0;
  p->len = mod->bytecode.len;

  gab_obj_closure *c = gab_obj_closure_create(p, NULL);

  gab_obj_function_set(f, GAB_VAL_NULL(), GAB_VAL_OBJ(c));

  add_constant(mod, GAB_VAL_OBJ(p));
  add_constant(mod, GAB_VAL_OBJ(c));
  return add_constant(mod, GAB_VAL_OBJ(f));
}

gab_module *gab_bc_compile(gab_engine *gab, s_i8 name, s_i8 source,
                           u8 narguments, s_i8 arguments[narguments]) {
  gab_module *mod = NEW(gab_module);
  gab_module_create(mod, name, source);

  gab_bc bc;
  gab_bc_create(&bc, source);

  i32 main = compile(gab, &bc, mod, name, narguments, arguments);

  if (main < 0) {
    DESTROY(mod);
    return NULL;
  }

  if (gab->flags & GAB_FLAG_DUMP_BYTECODE) {
    gab_module_dump(mod, name);
  }

  mod->main = main;
  mod->source_lines = bc.lex.source_lines;

  return mod;
}

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

static void dump_compiler_error(gab_bc *bc, gab_status e, const char *help_fmt,
                                ...) {
  if (bc->panic) {
    return;
  }

  bc->panic = true;

  va_list args;
  va_start(args, help_fmt);

  gab_lexer_finish_line(&bc->lex);

  gab_bc_frame *frame = &bc->frames[bc->frame_count - 1];
  s_i8 curr_token = bc->lex.previous_token_src;
  u64 curr_src_index = bc->line - 1;
  s_i8 curr_src;

  if (curr_src_index < bc->lex.source_lines->len) {
    curr_src = v_s_i8_val_at(bc->lex.source_lines, curr_src_index);
  } else {
    curr_src = s_i8_cstr("");
  }

  i8 *curr_src_start = curr_src.data;
  i32 curr_src_len = curr_src.len;

  while (is_whitespace(*curr_src_start)) {
    curr_src_start++;
    curr_src_len--;
    if (curr_src_len == 0)
      break;
  }

  s_i8 func_name = frame->locals_name[0];

  a_i8 *curr_under = a_i8_empty(curr_src_len);

  i8 *tok_start, *tok_end;

  tok_start = curr_token.data;
  tok_end = curr_token.data + curr_token.len;

  const char *tok = gab_token_names[bc->previous_token];

  for (u8 i = 0; i < curr_under->len; i++) {
    if (curr_src_start + i >= tok_start && curr_src_start + i < tok_end)
      curr_under->data[i] = '^';
    else
      curr_under->data[i] = ' ';
  }

  const char *curr_color = ANSI_COLOR_RED;

  const char *curr_box = "\u256d";

  fprintf(stderr,
          "[" ANSI_COLOR_GREEN "%.*s" ANSI_COLOR_RESET
          "] Error near " ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET
          ":\n\t%s%s %.4lu " ANSI_COLOR_RESET "%.*s"
          "\n\t\u2502      " ANSI_COLOR_YELLOW "%.*s" ANSI_COLOR_RESET
          "\n\t\u2570\u2500> ",
          (i32)func_name.len, func_name.data, tok, curr_box, curr_color,
          curr_src_index + 1, curr_src_len, curr_src_start,
          (i32)curr_under->len, curr_under->data);

  a_i8_destroy(curr_under);

  fprintf(stderr, ANSI_COLOR_YELLOW "%s. " ANSI_COLOR_RESET ANSI_COLOR_GREEN,
          gab_status_names[e]);

  vfprintf(stderr, help_fmt, args);

  fprintf(stderr, "\n" ANSI_COLOR_RESET);

  va_end(args);
}
