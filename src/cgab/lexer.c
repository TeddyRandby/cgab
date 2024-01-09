#include "lexer.h"
#include <stdio.h>

bool is_whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\f'; }

bool is_alpha_lower(uint8_t c) { return c >= 'a' && c <= 'z'; }

bool is_alpha_upper(uint8_t c) { return c >= 'A' && c <= 'Z'; }

bool is_alpha(uint8_t c) {
  return is_alpha_lower(c) || is_alpha_upper(c) || c == '_';
}

bool can_start_identifier(uint8_t c) { return is_alpha(c) || c == '_'; }

bool can_continue_identifier(uint8_t c) {
  return can_start_identifier(c) || c == '.';
}

bool is_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_comment(uint8_t c) { return c == '#'; }

typedef struct gab_lx {
  char *cursor;
  char *row_start;
  size_t row;
  uint64_t col;

  uint8_t nested_curly;
  uint8_t status;

  struct gab_src *source;

  s_char current_row_comment;
  s_char current_row_src;
  s_char current_token_src;
} gab_lx;

static void advance(gab_lx *self) {
  self->cursor++;
  self->col++;
  self->current_token_src.len++;
  self->current_row_src.len++;
}

static void start_row(gab_lx *self) {
  self->current_row_comment = (s_char){0};
  self->current_row_src.data = self->cursor;
  self->current_row_src.len = 0;
  self->col = 0;
  self->row++;
}

static void start_token(gab_lx *self) {
  self->current_token_src.data = self->cursor;
  self->current_token_src.len = 0;
}

static void finish_row(gab_lx *self) {
  if (self->current_row_src.len &&
      self->current_row_src.data[self->current_row_src.len - 1] == '\n')
    self->current_row_src.len--;

  v_s_char_push(&self->source->lines, self->current_row_src);

  start_row(self);
}

void gab_lexcreate(gab_lx *self, struct gab_src *src) {
  memset(self, 0, sizeof(gab_lx));

  self->source = src;
  self->cursor = src->source->data;
  self->row_start = src->source->data;

  start_row(self);
}

static inline char peek(gab_lx *self) { return *self->cursor; }

static inline char peek_next(gab_lx *self) { return *(self->cursor + 1); }

static inline gab_token error(gab_lx *self, enum gab_status s) {
  self->status = s;
  return TOKEN_ERROR;
}

typedef struct keyword {
  const char *literal;
  gab_token token;
} keyword;

const keyword keywords[] = {
    {
        "and",
        TOKEN_AND,
    },
    {

        "do",
        TOKEN_DO,
    },
    {

        "def",
        TOKEN_DEF,
    },
    {

        "else",
        TOKEN_ELSE,
    },
    {
        "end",
        TOKEN_END,
    },
    {
        "false",
        TOKEN_FALSE,
    },
    {
        "then",
        TOKEN_THEN,
    },
    {
        "not",
        TOKEN_NOT,
    },
    {
        "or",
        TOKEN_OR,
    },
    {
        "return",
        TOKEN_RETURN,
    },
    {
        "yield",
        TOKEN_YIELD,
    },
    {
        "true",
        TOKEN_TRUE,
    },
    {
        "nil",
        TOKEN_NIL,
    },
    {
        "loop",
        TOKEN_LOOP,
    },
    {
        "until",
        TOKEN_UNTIL,
    },
    {
        "break",
        TOKEN_BREAK,
    },
};

gab_token string(gab_lx *self) {
  uint8_t start = peek(self);
  uint8_t stop = start == '"' ? '"' : '\'';

  do {
    advance(self);

    if (peek(self) == '\0') {
      return error(self, GAB_MALFORMED_STRING);
    }

    if (start != '"') {
      if (peek(self) == '\n') {
        return error(self, GAB_MALFORMED_STRING);
      }

      if (peek(self) == '{') {
        advance(self);
        self->nested_curly++;
        return start == '}' ? TOKEN_INTERPOLATION_MIDDLE
                            : TOKEN_INTERPOLATION_BEGIN;
      }
    }
  } while (peek(self) != stop);

  // Eat the end
  advance(self);

  return start == '}' ? TOKEN_INTERPOLATION_END : TOKEN_STRING;
}

gab_token identifier(gab_lx *self) {
  while (can_continue_identifier(peek(self)))
    advance(self);

  if (peek(self) == '?' || peek(self) == '!')
    advance(self);

  for (int i = 0; i < sizeof(keywords) / sizeof(keyword); i++) {
    keyword k = keywords[i];
    s_char lit = s_char_create(k.literal, strlen(k.literal));
    if (s_char_match(self->current_token_src, lit)) {
      return k.token;
    }
  }

  return TOKEN_IDENTIFIER;
}

gab_token integer(gab_lx *self) {
  if (!is_digit(peek(self)))
    return error(self, GAB_MALFORMED_TOKEN);

  while (is_digit(peek(self)))
    advance(self);

  return TOKEN_NUMBER;
}

gab_token floating(gab_lx *self) {

  if (integer(self) == TOKEN_ERROR)
    return TOKEN_ERROR;

  if (peek(self) == '.' && is_digit(peek_next(self))) {
    advance(self);

    while (is_digit(peek(self))) {
      advance(self);
    }
  }

  return TOKEN_NUMBER;
}

#define CHAR_CASE(char, name)                                                  \
  case char: {                                                                 \
    advance(self);                                                             \
    return TOKEN_##name;                                                       \
  }

gab_token other(gab_lx *self) {
  switch (peek(self)) {

    CHAR_CASE('+', PLUS)
    CHAR_CASE('*', STAR)
    CHAR_CASE('/', SLASH)
    CHAR_CASE('%', PERCENT)
    CHAR_CASE('[', LBRACE)
    CHAR_CASE(']', RBRACE)
    CHAR_CASE('(', LPAREN)
    CHAR_CASE(')', RPAREN)
    CHAR_CASE(',', COMMA)
    CHAR_CASE(';', NEWLINE) // Treat as a line break for the compiler
    CHAR_CASE('|', PIPE)
    CHAR_CASE('?', QUESTION)
    CHAR_CASE('&', AMPERSAND)
    CHAR_CASE('!', BANG)

  case '{': {
    advance(self);
    if (self->nested_curly > 0)
      self->nested_curly++;
    return TOKEN_LBRACK;
  }

  case '}': {
    advance(self);
    if (self->nested_curly > 0)
      self->nested_curly--;
    return TOKEN_RBRACK;
  }

  case '@': {
    advance(self);

    if (is_digit(peek(self))) {
      if (integer(self) == TOKEN_NUMBER)
        return TOKEN_IMPLICIT;

      return error(self, GAB_MALFORMED_TOKEN);
    }

    return TOKEN_AT;
  }

  case '.': {
    advance(self);

    if (peek(self) == '.') {
      advance(self);
      return TOKEN_DOT_DOT;
    }

    if (can_continue_identifier(peek(self))) {
      if (identifier(self) == TOKEN_IDENTIFIER)
        return TOKEN_SIGIL;

      return error(self, GAB_MALFORMED_TOKEN);
    }

    return TOKEN_DOT;
  }
  case '-': {
    advance(self);
    switch (peek(self)) {
      CHAR_CASE('>', ARROW)
    default: {
      return TOKEN_MINUS;
    }
    }
  }

  case ':': {
    advance(self);

    if (can_start_identifier(peek(self))) {
      // If we didn't get a keyword, return a token message
      if (identifier(self) == TOKEN_IDENTIFIER) {
        // Messages can end in ? or !
        if (peek(self) == '?' || peek(self) == '!') {
          advance(self);
        }

        return TOKEN_MESSAGE;
      }

      // Otherwise, we got a keyword and this was an error
      return error(self, GAB_MALFORMED_TOKEN);
    }

    switch (peek(self)) {
      CHAR_CASE('+', MESSAGE)
      CHAR_CASE('-', MESSAGE)
      CHAR_CASE('*', MESSAGE)
      CHAR_CASE('/', MESSAGE)
      CHAR_CASE('%', MESSAGE)
      CHAR_CASE('&', MESSAGE)
      CHAR_CASE('|', MESSAGE)
    case '[': {
      advance(self);

      if (peek(self) == ']') {
        advance(self);

        return TOKEN_MESSAGE;
      }

      if (peek(self) == '=') {
        advance(self);
        if (peek(self) == ']') {
          advance(self);
          return TOKEN_MESSAGE;
        }
      }

      return TOKEN_ERROR;
    }
    case '(': {
      if (peek_next(self) == ')') {
        advance(self);
        advance(self);

        return TOKEN_MESSAGE;
      }

      return TOKEN_COLON;
    }
    case '=': {
      advance(self);

      if (peek(self) == '=') {
        advance(self);
        return TOKEN_MESSAGE;
      }

      return TOKEN_COLON_EQUAL;
    }
    case '<': {
      advance(self);

      switch (peek(self)) {
      case '<':
      case '=':
        advance(self);
      }

      return TOKEN_MESSAGE;
    }
    case '>': {
      advance(self);

      switch (peek(self)) {
      case '>':
      case '=':
        advance(self);
      }

      return TOKEN_MESSAGE;
    }
    default: {
      return TOKEN_COLON;
    }
    }
  }

  case '=': {
    advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', EQUAL_EQUAL)
      CHAR_CASE('>', FAT_ARROW)
    default: {
      return TOKEN_EQUAL;
    }
    }
  }
  case '<': {
    advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', LESSER_EQUAL)
      CHAR_CASE('<', LESSER_LESSER)
    default: {
      return TOKEN_LESSER;
    }
    }
  }
  case '>': {
    advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', GREATER_EQUAL)
      CHAR_CASE('>', GREATER_GREATER)
    default: {
      return TOKEN_GREATER;
    }
    }
  }
  default: {
    advance(self);
    return error(self, GAB_MALFORMED_TOKEN);
  }
  }
}

static inline void parse_comment(gab_lx *self) {
  while (is_comment(peek(self))) {
    while (peek(self) != '\n') {
      advance(self);

      if (peek_next(self) == '\0' || peek_next(self) == EOF)
        break;
    }

    advance(self);
    finish_row(self);
  }
}

gab_token gab_lexnxt(gab_lx *self) {
  while (is_whitespace(peek(self)) || is_comment(peek(self))) {
    if (is_comment(peek(self)))
      parse_comment(self);

    if (is_whitespace(peek(self)))
      advance(self);
  }

  // Sanity check
  assert(self->cursor - self->source->source->data < self->source->source->len);

  gab_token tok;
  start_token(self);

  if (peek(self) == '\0' || peek(self) == EOF) {
    finish_row(self);
    tok = TOKEN_EOF;
    goto fin;
  }

  if (peek(self) == '\n') {
    advance(self);
    finish_row(self);
    tok = TOKEN_NEWLINE;
    goto fin;
  }

  if (is_alpha(peek(self))) {
    tok = identifier(self);
    goto fin;
  }

  if (is_digit(peek(self))) {
    tok = floating(self);
    goto fin;
  }

  if (peek(self) == '"') {
    tok = string(self);
    goto fin;
  }

  if (peek(self) == '\'') {
    tok = string(self);
    goto fin;
  }

  if (self->nested_curly == 1 && peek(self) == '}') {
    self->nested_curly--;
    tok = string(self);
    goto fin;
  }

  tok = other(self);

fin:
  v_gab_token_push(&self->source->tokens, tok);
  v_s_char_push(&self->source->token_srcs, self->current_token_src);
  v_uint64_t_push(&self->source->token_lines,
                  self->row - (tok == TOKEN_NEWLINE));

  return tok;
}

struct gab_src *gab_srccpy(struct gab_triple gab, struct gab_src *self) {
  gab_value name = gab_valcpy(gab, self->name);

  if (d_gab_src_exists(&gab.eg->sources, name))
    return d_gab_src_read(&gab.eg->sources, name);

  struct gab_src *copy = NEW(struct gab_src);
  memset(copy, 0, sizeof(struct gab_src));

  d_gab_src_insert(&gab.eg->sources, name, copy);

  copy->name = name;
  copy->source = a_char_create(self->source->data, self->source->len);

  v_s_char_copy(&copy->lines, &self->lines);

  v_gab_token_copy(&copy->tokens, &self->tokens);
  v_s_char_copy(&copy->token_srcs, &self->token_srcs);
  v_uint64_t_copy(&copy->token_lines, &self->token_lines);

  v_uint8_t_copy(&copy->bytecode, &self->bytecode);
  v_uint64_t_copy(&copy->bytecode_toks, &self->bytecode_toks);
  v_gab_value_copy(&copy->constants, &self->constants);

  // Reconcile the constant array by copying the non trivial values
  for (size_t i = 0; i < self->constants.len; i++) {
    gab_value v = v_gab_value_val_at(&self->constants, i);
    if (gab_valiso(v)) {
      gab_value cpy = gab_valcpy(gab, v);
      gab_egkeep(gab.eg, gab_iref(gab, cpy));
      v_gab_value_set(&copy->constants, i, cpy);
    }
  }

  // Reconcile the copied slices to point to the new source
  for (uint64_t i = 0; i < copy->lines.len; i++) {
    s_char *copy_src = v_s_char_ref_at(&copy->lines, i);
    s_char *src_src = v_s_char_ref_at(&self->lines, i);

    copy_src->data = copy->source->data + (src_src->data - self->source->data);
  }

  return copy;
}

void gab_srcdestroy(struct gab_src *self) {
  a_char_destroy(self->source);

  v_s_char_destroy(&self->lines);

  v_gab_token_destroy(&self->tokens);
  v_s_char_destroy(&self->token_srcs);
  v_uint64_t_destroy(&self->token_lines);

  v_gab_value_destroy(&self->constants);

  v_uint8_t_destroy(&self->bytecode);
  v_uint64_t_destroy(&self->bytecode_toks);

  DESTROY(self);
}

struct gab_src *gab_src(struct gab_triple gab, gab_value name,
                        const char *source, size_t len) {
  if (d_gab_src_exists(&gab.eg->sources, name))
    return d_gab_src_read(&gab.eg->sources, name);

  struct gab_src *src = NEW(struct gab_src);
  memset(src, 0, sizeof(struct gab_src));

  src->source = a_char_create(source, len);
  src->name = name;

  gab_egkeep(gab.eg, gab_iref(gab, name));

  gab_lx lex;
  gab_lexcreate(&lex, src);

  for (;;) {
    gab_token t = gab_lexnxt(&lex);
    if (t == TOKEN_EOF)
      break;
  }

  d_gab_src_insert(&gab.eg->sources, name, src);

  return src;
}

#undef CURSOR
#undef NEXT_CURSOR
#undef ADVANCE
