#include "lexer.h"
#include "char.h"
#include "token.h"
#include <stdio.h>

static void cursor_advance(gab_lexer *self) {
  self->cursor++;
  self->col++;
  self->current_token_src.size++;
  self->current_row_src.size++;
}

static void start_row(gab_lexer *self) {
  self->current_row_src.data = self->cursor;
  self->current_row_src.size = 0;
  self->col = 0;
  self->row++;
  self->previous_row = self->current_row;
}

static void start_token(gab_lexer *self) {
  self->current_token_src.data = self->cursor;
  self->current_token_src.size = 0;
}

static void finish_row(gab_lexer *self) {
  self->skip_lines++;

  self->previous_row_src = self->current_row_src;
  // Skip the newline at the end of the row.
  self->previous_row_src.size--;
  v_s_u8_ref_push(self->source_lines, self->previous_row_src);
  start_row(self);
}

void gab_lexer_create(gab_lexer *self, s_u8_ref src) {
  self->source = src;
  self->cursor = src.data;
  self->row_start = src.data;
  self->nested_curly = 0;
  self->current_row = 1;
  self->skip_lines = 0;

  self->row = 0;
  self->col = 0;

  self->source_lines = CREATE_STRUCT(v_s_u8_ref);
  v_s_u8_ref_create(self->source_lines);
  start_row(self);
}

static inline u8 peek(gab_lexer *self) { return *self->cursor; }

static inline u8 peek_next(gab_lexer *self) { return *(self->cursor + 1); }

gab_token return_error(gab_lexer *self, const char *msg) {
  self->error_msg = msg;
  return TOKEN_ERROR;
}

gab_token identifier(gab_lexer *self) {
  start_token(self);

  while (is_alpha(peek(self)) || is_digit(peek(self))) {
    cursor_advance(self);
  }

  switch (self->current_token_src.data[0]) {
  case 'a': {
    if (s_u8_ref_match_lit(self->current_token_src, "and")) {
      return TOKEN_AND;
    }
  }
  case 'd': {
    if (s_u8_ref_match_lit(self->current_token_src, "do")) {
      return TOKEN_DO;
    }
    if (s_u8_ref_match_lit(self->current_token_src, "def")) {
      return TOKEN_DEF;
    }
  }
  case 'e': {
    switch (self->current_token_src.data[1]) {
    case 'l': {
      if (s_u8_ref_match_lit(self->current_token_src, "else")) {
        return TOKEN_ELSE;
      }
    }
    case 'n': {
      if (s_u8_ref_match_lit(self->current_token_src, "end")) {
        return TOKEN_END;
      }
    }
    }
  }
  case 'f': {
    switch (self->current_token_src.data[1]) {
    case 'a': {
      if (s_u8_ref_match_lit(self->current_token_src, "false")) {
        return TOKEN_FALSE;
      }
    }
    case 'o': {
      if (s_u8_ref_match_lit(self->current_token_src, "for")) {
        return TOKEN_FOR;
      }
    }
    }
  }
  case 'i': {
    switch (self->current_token_src.data[1]) {
    case 'f': {
      if (s_u8_ref_match_lit(self->current_token_src, "if")) {
        return TOKEN_IF;
      }
    }
    case 'n': {
      if (s_u8_ref_match_lit(self->current_token_src, "in")) {
        return TOKEN_IN;
      }
    }
    case 's': {
      if (s_u8_ref_match_lit(self->current_token_src, "is")) {
        return TOKEN_IS;
      }
    }
    }
  }
  case 'l': {
    if (s_u8_ref_match_lit(self->current_token_src, "let")) {
      return TOKEN_LET;
    }
  }
  case 'm': {
    if (s_u8_ref_match_lit(self->current_token_src, "match")) {
      return TOKEN_MATCH;
    }
  }
  case 'n': {
    switch (self->current_token_src.data[1]) {
    case 'u': {
      if (s_u8_ref_match_lit(self->current_token_src, "null")) {
        return TOKEN_NULL;
      }
    }
    case 'o': {
      if (s_u8_ref_match_lit(self->current_token_src, "not")) {
        return TOKEN_NOT;
      }
    }
    }
  }
  case 'o': {
    if (s_u8_ref_match_lit(self->current_token_src, "or")) {
      return TOKEN_OR;
    }
  }
  case 'r': {
    if (s_u8_ref_match_lit(self->current_token_src, "return")) {
      return TOKEN_RETURN;
    }
  }
  case 't': {
    switch (self->current_token_src.data[1]) {
    case 'r': {
      if (s_u8_ref_match_lit(self->current_token_src, "true")) {
        return TOKEN_TRUE;
      }
    }
    case 'h': {
      if (s_u8_ref_match_lit(self->current_token_src, "then")) {
        return TOKEN_THEN;
      }
    }
    }
  }
  case 'w': {
    if (s_u8_ref_match_lit(self->current_token_src, "while")) {
      return TOKEN_WHILE;
    }
  }
  default: {
    return TOKEN_IDENTIFIER;
  }
  }
}

gab_token string(gab_lexer *self) {
  start_token(self);

  u8 start = peek(self);
  u8 stop = start == '"' ? '"' : '\'';

  do {
    cursor_advance(self);

    if (peek(self) == '\0') {
      return return_error(self, "Unexpected EOF in string literal");
    }

    if (start != '"') {
      if (peek(self) == '\n') {
        return return_error(self, "Unexpected New Line in string literal");
      }

      if (peek(self) == '{') {
        cursor_advance(self);
        self->nested_curly++;
        return TOKEN_INTERPOLATION;
      }
    }
  } while (peek(self) != stop);

  // Eat the end
  cursor_advance(self);

  return start == '}' ? TOKEN_INTERPOLATION_END : TOKEN_STRING;
}

gab_token number(gab_lexer *self) {
  start_token(self);

  while (is_digit(peek(self))) {
    cursor_advance(self);
  }

  if (peek(self) == '.' && is_digit(peek_next(self))) {
    cursor_advance(self);

    while (is_digit(peek(self))) {
      cursor_advance(self);
    }
  }

  return TOKEN_NUMBER;
}

#define CHAR_CASE(char, name)                                                  \
  case char: {                                                                 \
    cursor_advance(self);                                                      \
    return TOKEN_##name;                                                       \
  }

gab_token other(gab_lexer *self) {

  start_token(self);

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
    CHAR_CASE(';', SEMICOLON)
    CHAR_CASE('|', PIPE)
    CHAR_CASE('?', QUESTION)
    CHAR_CASE('&', AMPERSAND)
    CHAR_CASE('!', BANG)
    CHAR_CASE('@', AT)
  case '{': {
    cursor_advance(self);
    if (self->nested_curly > 0)
      self->nested_curly++;
    return TOKEN_LBRACK;
  }
  case '}': {
    cursor_advance(self);
    if (self->nested_curly > 0)
      self->nested_curly--;
    return TOKEN_RBRACK;
  }
  case '.': {
    cursor_advance(self);
    switch (peek(self)) {
      CHAR_CASE('.', DOT_DOT);
    default: {
      return TOKEN_DOT;
    }
    }
  }
  case '-': {
    cursor_advance(self);
    switch (peek(self)) {
      CHAR_CASE('>', ARROW)
    default: {
      return TOKEN_MINUS;
    }
    }
  }
  case ':': {
    cursor_advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', COLON_EQUAL)
    default: {
      return TOKEN_COLON;
    }
    }
  }
  case '=': {
    cursor_advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', EQUAL_EQUAL)
      CHAR_CASE('>', FAT_ARROW)
    default: {
      return TOKEN_EQUAL;
    }
    }
  }
  case '<': {
    cursor_advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', LESSER_EQUAL)
    default: {
      return TOKEN_LESSER;
    }
    }
  }
  case '>': {
    cursor_advance(self);
    switch (peek(self)) {
      CHAR_CASE('=', GREATER_EQUAL)
    default: {
      return TOKEN_GREATER;
    }
    }
  }
  default: {
    cursor_advance(self);
    return return_error(self, "Unknown character");
  }
  }
}

static inline void parse_comment(gab_lexer *self) {
  while (peek(self) != '\n') {
    cursor_advance(self);
  }
}

static inline void parse_whitespace(gab_lexer *self) { cursor_advance(self); }

void handle_ignored(gab_lexer *self) {
  while (is_whitespace(peek(self)) || is_comment(peek(self))) {
    if (is_comment(peek(self))) {
      parse_comment(self);
    } else if (is_whitespace(peek(self))) {
      parse_whitespace(self);
    }
  }
}

gab_token gab_lexer_next(gab_lexer *self) {
  // copy current into previous.
  self->previous_token_src = self->current_token_src;

  handle_ignored(self);

  if (peek(self) == '\0') {
    return TOKEN_EOF;
  }

  if (peek(self) == '\n') {
    cursor_advance(self);
    finish_row(self);
    return TOKEN_NEWLINE;
  }

  self->current_row += self->skip_lines;
  self->skip_lines = 0;

  if (is_alpha(peek(self))) {
    return identifier(self);
  }

  if (is_digit(peek(self))) {
    return number(self);
  }

  if (peek(self) == '"') {
    return string(self);
  }

  if (peek(self) == '\'') {
    return string(self);
  }

  if (self->nested_curly == 1 && peek(self) == '}') {
    self->nested_curly--;
    return string(self);
  }

  return other(self);
}

void gab_lexer_finish_line(gab_lexer *self) {
  while (peek(self) != '\n' && peek(self) != '\0') {
    cursor_advance(self);
  }

  cursor_advance(self);
  finish_row(self);

  self->current_row += self->skip_lines;
  self->skip_lines = 0;
};

#undef CURSOR
#undef NEXT_CURSOR
#undef ADVANCE
