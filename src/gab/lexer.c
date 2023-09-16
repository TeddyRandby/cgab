#include "include/lexer.h"
#include "include/char.h"
#include "include/engine.h"
#include "include/types.h"
#include <stdio.h>

typedef struct gab_lx {
  int8_t *cursor;
  int8_t *row_start;
  size_t row;
  uint64_t col;

  uint8_t nested_curly;
  uint8_t status;

  gab_src *source;

  s_int8_t current_row_comment;
  s_int8_t current_row_src;
  s_int8_t current_token_src;
} gab_lx;

static void advance(gab_lx *self) {
  self->cursor++;
  self->col++;
  self->current_token_src.len++;
  self->current_row_src.len++;
}

static void start_row(gab_lx *self) {
  self->current_row_comment = (s_int8_t){0};
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
  if (self->current_row_src.data[self->current_row_src.len - 1] == '\n')
    self->current_row_src.len--;

  v_s_int8_t_push(&self->source->lines, self->current_row_src);
  v_s_int8_t_push(&self->source->line_comments, self->current_row_comment);

  start_row(self);
}

void gab_lexcreate(gab_lx *self, gab_src *src) {
  memset(self, 0, sizeof(gab_lx));

  self->source = src;
  self->cursor = src->source->data;
  self->row_start = src->source->data;

  start_row(self);
}

static inline int8_t peek(gab_lx *self) { return *self->cursor; }

static inline int8_t peek_next(gab_lx *self) { return *(self->cursor + 1); }

static inline gab_token error(gab_lx *self, gab_status s) {
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
        "match",
        TOKEN_MATCH,
    },
    {
        "for",
        TOKEN_FOR,
    },
    {
        "then",
        TOKEN_THEN,
    },
    {
        "in",
        TOKEN_IN,
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
};

gab_token identifier(gab_lx *self) {
  while (is_alpha(peek(self)) || is_digit(peek(self)))
    advance(self);

  if (peek(self) == '?' || peek(self) == '!')
    advance(self);

  for (int32_t i = 0; i < sizeof(keywords) / sizeof(keyword); i++) {
    keyword k = keywords[i];
    s_int8_t lit = s_int8_t_create((int8_t *)k.literal, strlen(k.literal));
    if (s_int8_t_match(self->current_token_src, lit)) {
      return k.token;
    }
  }

  return TOKEN_IDENTIFIER;
}

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
        return TOKEN_INTERPOLATION;
      }
    }
  } while (peek(self) != stop);

  // Eat the end
  advance(self);

  return start == '}' ? TOKEN_INTERPOLATION_END : TOKEN_STRING;
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

    if (is_alpha(peek(self))) {
      // If we didn't get a keyword, return a token message
      if (identifier(self) == TOKEN_IDENTIFIER)
        return TOKEN_PROPERTY;

      // Otherwise, we got a keyword and this was an error

      return error(self, GAB_MALFORMED_TOKEN);
    }

    switch (peek(self)) {
      CHAR_CASE('.', DOT_DOT);
    default: {
      return TOKEN_DOT;
    }
    }
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

    if (is_alpha(peek(self))) {
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
      CHAR_CASE('=', COLON_EQUAL)
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
  int8_t *start = self->cursor;

  while (is_comment(peek(self))) {
    while (peek(self) != '\n')
      advance(self);

    advance(self);
  }

  self->current_row_comment = s_int8_t_create(start, self->cursor - start);
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
  v_s_int8_t_push(&self->source->tokens_src, self->current_token_src);
  v_uint64_t_push(&self->source->tokens_line, self->row);

  return tok;
}

gab_src *gab_srccreate(gab_eg *gab, s_int8_t source) {
  gab_src *self = NEW(gab_src);
  memset(self, 0, sizeof(gab_src));

  self->source = a_int8_t_create(source.data, source.len);
  self->next = gab->sources;
  gab->sources = self;

  return self;
}

gab_src *gab_srccpy(gab_eg *gab, gab_src *self) {
  gab_src *copy = NEW(gab_src);
  copy->source = a_int8_t_create(self->source->data, self->source->len);

  v_s_int8_t_copy(&copy->lines, &self->lines);

  // Reconcile the copied slices to point to the new source
  for (uint64_t i = 0; i < copy->lines.len; i++) {
    s_int8_t *copy_src = v_s_int8_t_ref_at(&copy->lines, i);
    s_int8_t *src_src = v_s_int8_t_ref_at(&self->lines, i);

    copy_src->data = copy->source->data + (src_src->data - self->source->data);
  }

  copy->next = gab->sources;
  gab->sources = copy;

  return copy;
}

void gab_srcfree(gab_src *self) {
  v_s_int8_t_destroy(&self->lines);
  a_int8_t_destroy(self->source);
  DESTROY(self);
}

gab_src *gab_lex(gab_eg *gab, const char *source, size_t len) {
  gab_src *src = gab_srccreate(gab, s_int8_t_create((int8_t *)source, len));

  gab_lx lex;
  gab_lexcreate(&lex, src);

  for (;;) {
    gab_token t = gab_lexnxt(&lex);
    if (t == TOKEN_EOF)
      break;
  }

  return src;
}

#undef CURSOR
#undef NEXT_CURSOR
#undef ADVANCE
