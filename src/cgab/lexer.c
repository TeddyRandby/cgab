#include "lexer.h"
#include "core.h"
#include "gab.h"
#include <threads.h>

bool is_whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\f'; }

bool is_alpha_lower(uint8_t c) { return c >= 'a' && c <= 'z'; }

bool is_alpha_upper(uint8_t c) { return c >= 'A' && c <= 'Z'; }

bool is_alpha(uint8_t c) {
  return is_alpha_lower(c) || is_alpha_upper(c) || c == '_';
}

bool can_start_symbol(uint8_t c) { return is_alpha(c) || c == '_'; }

bool can_continue_symbol(uint8_t c) { return can_start_symbol(c) || c == '.'; }

bool can_end_symbol(uint8_t c) { return c == '?' || c == '!'; }

bool can_start_operator(uint8_t c) {
  switch (c) {
  case '!':
  case '$':
  case '%':
  case '^':
  case '*':
  case '/':
  case '+':
  case '-':
  case '&':
  case '|':
  case '=':
  case '<':
  case '>':
  case '?':
  case '~':
  case '@':
    return true;
  default:
    return false;
  }
}

bool can_continue_operator(uint8_t c) {
  switch (c) {
  default:
    return can_start_operator(c);
  }
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

  v_gab_value_push(&src->constants, gab_nil);
  v_gab_value_push(&src->constants, gab_false);
  v_gab_value_push(&src->constants, gab_true);
  v_gab_value_push(&src->constants, gab_ok);
  v_gab_value_push(&src->constants, gab_err);
  v_gab_value_push(&src->constants, gab_none);

  start_row(self);
}

static inline char peek(gab_lx *self) { return *self->cursor; }

static inline char peek_next(gab_lx *self) { return *(self->cursor + 1); }

static inline gab_token lexer_error(gab_lx *self, enum gab_status s) {
  self->status = s;
  return TOKEN_ERROR;
}

typedef struct keyword {
  const char *literal;
  gab_token token;
} keyword;

const keyword keywords[] = {
    {

        "do",
        TOKEN_DO,
    },
    {
        "end",
        TOKEN_END,
    },
};

gab_token end_symbol(gab_lx *self) {
  switch (peek(self)) {
  case '?':
  case '!':
    advance(self);
    return TOKEN_SYMBOL;

  case '[':
    advance(self);

    if (peek(self) == ']')
      return advance(self), TOKEN_SYMBOL;

    break;

  case '{':
    advance(self);

    if (peek(self) == '}')
      return advance(self), TOKEN_SYMBOL;

    break;
  }

  return lexer_error(self, GAB_MALFORMED_TOKEN);
}

gab_token string(gab_lx *self) {
  uint8_t start = peek(self);
  uint8_t stop = start == '"' ? '"' : '\'';

  do {
    advance(self);

    if (peek(self) == '\0')
      return lexer_error(self, GAB_MALFORMED_STRING);

    if (start != '"')
      if (peek(self) == '\n')
        return lexer_error(self, GAB_MALFORMED_STRING);

  } while (peek(self) != stop);

  advance(self);
  return TOKEN_STRING;
}

gab_token operator(gab_lx *self) {
  while (can_continue_operator(peek(self)))
    advance(self);

  return TOKEN_OPERATOR;
}

gab_token symbol(gab_lx *self) {
  while (can_continue_symbol(peek(self)))
    advance(self);

  for (int i = 0; i < sizeof(keywords) / sizeof(keyword); i++) {
    keyword k = keywords[i];
    s_char lit = s_char_create(k.literal, strlen(k.literal));
    if (s_char_match(self->current_token_src, lit)) {
      return k.token;
    }
  }

  if (can_end_symbol(peek(self)))
    return end_symbol(self);

  return TOKEN_SYMBOL;
}

gab_token integer(gab_lx *self) {
  if (!is_digit(peek(self)))
    return lexer_error(self, GAB_MALFORMED_TOKEN);

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

gab_token other(gab_lx *self) {
  switch (peek(self)) {
  case ';':
    advance(self);
    return TOKEN_NEWLINE;
  case ',':
    advance(self);
    return TOKEN_NEWLINE;
  case '(':
    advance(self);
    return TOKEN_LPAREN;
  case ')':
    advance(self);
    return TOKEN_RPAREN;
  case '[':
    advance(self);
    return TOKEN_LBRACE;
  case ']':
    advance(self);
    return TOKEN_RBRACE;

  case '{':
    advance(self);

    if (self->nested_curly > 0)
      self->nested_curly++;

    return TOKEN_LBRACK;

  case '}':
    advance(self);

    if (self->nested_curly > 0)
      self->nested_curly--;

    return TOKEN_RBRACK;

  case '\\':
    advance(self);

    if (can_start_operator(peek(self))) {
      advance(self);

      enum gab_token t = operator(self);

      if (t == TOKEN_OPERATOR)
        return TOKEN_MESSAGE;

      return lexer_error(self, GAB_MALFORMED_SEND);
    }

    if (can_start_symbol(peek(self))) {
      advance(self);

      enum gab_token t = symbol(self);

      if (t == TOKEN_SYMBOL)
        return TOKEN_MESSAGE;

      return lexer_error(self, GAB_MALFORMED_TOKEN);
    }

    return TOKEN_MESSAGE;

  case ':':
    advance(self);

    if (can_start_operator(peek(self))) {
      advance(self);

      enum gab_token t = operator(self);

      if (t == TOKEN_OPERATOR)
        return TOKEN_SEND;

      return lexer_error(self, GAB_MALFORMED_SEND);
    }

    if (can_start_symbol(peek(self))) {
      advance(self);

      enum gab_token t = symbol(self);

      if (t == TOKEN_SYMBOL)
        return TOKEN_SEND;

      return lexer_error(self, GAB_MALFORMED_SEND);
    }

    return TOKEN_SEND;

  case '.':
    advance(self);

    if (can_continue_symbol(peek(self))) {
      enum gab_token t = symbol(self);

      if (t == TOKEN_SYMBOL)
        return TOKEN_SIGIL;

      return lexer_error(self, GAB_MALFORMED_TOKEN);
    }

    return lexer_error(self, GAB_MALFORMED_TOKEN);

  default:
    if (can_start_operator(peek(self)))
      return operator(self);

    advance(self);
    return lexer_error(self, GAB_MALFORMED_TOKEN);
  }
}

static inline void parse_comment(gab_lx *self) {
  while (peek(self) != '\n') {
    advance(self);

    if (peek_next(self) == '\0' || peek_next(self) == EOF)
      break;
  }

  advance(self);
  finish_row(self);
}

gab_token gab_lexnext(gab_lx *self) {
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
    tok = symbol(self);
    goto fin;
  }

  if (peek(self) == '-' && is_digit(peek_next(self))) {
    advance(self);
    tok = floating(self);
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
  v_uint64_t_push(&self->source->token_lines, self->row);

  return tok;
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

  for (size_t i = 0; i < self->len; i++) {
    if (self->thread_bytecode[i].constants)
      free(self->thread_bytecode[i].constants);
    if (self->thread_bytecode[i].bytecode)
      free(self->thread_bytecode[i].bytecode);
  }

  free(self);
}

struct gab_src *gab_src(struct gab_triple gab, gab_value name,
                        const char *source, size_t len) {
  mtx_lock(&gab.eg->sources_mtx);

  if (d_gab_src_exists(&gab.eg->sources, name)) {
    struct gab_src *src = d_gab_src_read(&gab.eg->sources, name);

    mtx_unlock(&gab.eg->sources_mtx);

    return src;
  }

  size_t sz =
      sizeof(struct gab_src) + (gab.eg->len - 1) * sizeof(struct src_bytecode);

  struct gab_src *src = malloc(sz);
  memset(src, 0, sz);

  src->len = gab.eg->len - 1;
  src->source = a_char_create(source, len);
  src->name = name;

  gab_egkeep(gab.eg, gab_iref(gab, name));

  if (!len)
    goto fin;

  gab_lx lex;
  gab_lexcreate(&lex, src);

  for (;;) {
    gab_token t = gab_lexnext(&lex);
    if (t == TOKEN_EOF)
      break;
  }

fin:
  d_gab_src_insert(&gab.eg->sources, name, src);

  mtx_unlock(&gab.eg->sources_mtx);

  return src;
}

size_t gab_srcappend(struct gab_src *self, size_t len, uint8_t bc[static len],
                     uint64_t toks[static len]) {
  v_uint8_t_cap(&self->bytecode, self->bytecode.len + len);
  v_uint64_t_cap(&self->bytecode_toks, self->bytecode_toks.len + len);

  for (size_t i = 0; i < len; i++) {
    v_uint8_t_push(&self->bytecode, bc[i]);
    v_uint64_t_push(&self->bytecode_toks, toks[i]);
  }

  assert(self->bytecode.len == self->bytecode_toks.len);

  return self->bytecode.len;
}

gab_value gab_srcname(struct gab_src *src) { return src->name; }

size_t gab_srcline(struct gab_src *src, size_t offset) {
  size_t tok = v_uint64_t_val_at(&src->bytecode_toks, offset);
  return v_uint64_t_val_at(&src->token_lines, tok);
}

#undef CURSOR
#undef NEXT_CURSOR
#undef ADVANCE
