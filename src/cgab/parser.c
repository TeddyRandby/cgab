#include "colors.h"
#include "core.h"
#define GAB_TOKEN_NAMES_IMPL
#include "engine.h"
#include "gab.h"
#include "lexer.h"

#define FMT_EXPECTED_EXPRESSION                                                \
  "Expected a value - one of:\n\n"                                             \
  "  " GAB_YELLOW "-1.23" GAB_MAGENTA "\t\t\t# A number \n" GAB_RESET          \
  "  " GAB_GREEN ".true" GAB_MAGENTA "\t\t\t# A sigil \n" GAB_RESET            \
  "  " GAB_GREEN "'hello, Joe!'" GAB_MAGENTA "\t\t# A string \n" GAB_RESET     \
  "  " GAB_RED "\\greet" GAB_MAGENTA "\t\t# A message\n" GAB_RESET             \
  "  " GAB_BLUE "do x; x + 1 end" GAB_MAGENTA "\t# A block \n" GAB_RESET       \
  "  " GAB_CYAN "{ key = value }" GAB_MAGENTA "\t# A record\n" GAB_RESET "  "  \
  "(" GAB_YELLOW "-1.23" GAB_RESET ", " GAB_GREEN ".true" GAB_RESET            \
  ")" GAB_MAGENTA "\t# A tuple\n" GAB_RESET "  "                               \
  "a_variable" GAB_MAGENTA "\t\t# Or a variable!\n" GAB_RESET

#define FMT_CLOSING_RBRACE                                                     \
  "Expected a closing $ to define a rest assignment target."

#define FMT_EXTRA_REST_TARGET                                                  \
  "$ is already a rest-target.\n"                                              \
  "\nBlocks and assignments can only declare one target as a rest-target.\n"

#define FMT_UNEXPECTEDTOKEN "Expected $ instead."

#define FMT_REFERENCE_BEFORE_INIT "$ is referenced before it is initialized."

#define FMT_ID_NOT_FOUND "Variable $ is not defined in this scope."

#define FMT_ASSIGNMENT_ABANDONED                                               \
  "This assignment expression is incomplete.\n\n"                              \
  "Assignments consist of a list of targets and a list of values, separated "  \
  "by an $.\n\n"                                                               \
  "  a, b = " GAB_YELLOW "1" GAB_RESET ", " GAB_YELLOW "2\n" GAB_RESET         \
  "  a:put!(" GAB_GREEN ".key" GAB_RESET "), b = " GAB_YELLOW "1" GAB_RESET    \
  ", " GAB_YELLOW "2\n" GAB_RESET

/*
 *******
 * AST *
 *******

  The gab ast is built of two kinds of nodes:
    - Sends  (behavior)
    - Values (data)

  VALUE NODE

  1       => [ 1 ]
  (1,2,3) => [1, 2, 3]

  Simply a list of 0 or more values

  SEND VALUE

  1 + 1   => [{ gab.lhs: [1], gab.msg: +, gab.rhs: [1] }]

  [ 1, 2 ] => [{ gab.lhs: [:gab.list], gab.msg: make, gab.rhs: [1, 2] }]

  Simply a record with a lhs, rhs, and msg.

  BLOCK VALUE

  do            => [[
    b .= 1 + 1       { gab.lhs: [b], gab.msg: =, gab.rhs: [{ 1, b, 1}] },
    b.toString       { gab.lhs: [b], gab.msg: toString, gab.rhs: [] }
  end             ]]

  And thats it! All Gab code can be described here. There are some nuances
 though:

  (1 + 2, 3):print =>
    { [ { [ { [ 1 2 ], \+ } ], \gab.runtime.trim ] 3 ], \print }

  a = 2 => { [this, \a, 2], \gab.runtime.put! }

  Sometimes we need to call special messages in the runtime. There are three
 special cases so far:

    \gab.runtime.trim : This is a special message for trimming values up and
 down on the stack. Since gab has multiple return values, any expression can
 return any number of values. At each callsite, we need to specify how many
 values we need returned, and adjust accordingly

    \gab.runtime.pack : Blocks and assignments can declare _rest_ parameters,
 which collect all extra values, like so: In place of a call to
 \gab.runtime.trim, we emit a call to \gab.runtime.pack, and include special
 arguments for how to pack the values.

      a, b[] = 1,2,3,4,5 => 1, [2 3 4 5]

    \gab.runtime.put! : This one is still a little fuzzy / up for debate. We
 need to express variable assignment as a send. The following changes describe
 this implementation:

      - Blocks (more specifically, prototypes) are given a shape just like
 records have.
      - gab_unquote() accepts a *shape* as an argument. This shape determines
 the environment available as the AST is compiled into a block.
          -> How does this handle nested scopes and chaining?
          -> How do we implement load_local/load_upvalue
 */

struct parser {
  struct gab_src *src;
  size_t offset;
};

enum prec_k { kNONE, kASSIGNMENT, kBINARY_SEND, kSEND, kPRIMARY };

typedef gab_value (*parse_f)(struct gab_triple gab, struct parser *,
                             gab_value lhs);

struct parse_rule {
  parse_f prefix;
  parse_f infix;
  enum prec_k prec;
};

struct parse_rule get_parse_rule(gab_token k);

static size_t prev_line(struct parser *parser) {
  return v_uint64_t_val_at(&parser->src->token_lines, parser->offset - 1);
}

static gab_token curr_tok(struct parser *parser) {
  return v_gab_token_val_at(&parser->src->tokens, parser->offset);
}

static bool curr_prefix(struct parser *parser, enum prec_k prec) {
  return get_parse_rule(curr_tok(parser)).prefix != nullptr;
}

static gab_token prev_tok(struct parser *parser) {
  return v_gab_token_val_at(&parser->src->tokens, parser->offset - 1);
}

static s_char prev_src(struct parser *parser) {
  return v_s_char_val_at(&parser->src->token_srcs, parser->offset - 1);
}

static gab_value prev_id(struct gab_triple gab, struct parser *parser) {
  s_char s = prev_src(parser);

  return gab_nstring(gab, s.len, s.data);
}

static gab_value tok_id(struct gab_triple gab, gab_token tok) {
  // These can cause collections during compilation.
  return gab_string(gab, gab_token_names[tok]);
}

static int encode_codepoint(char *out, int utf) {
  if (utf <= 0x7F) {
    // Plain ASCII
    out[0] = (char)utf;
    return 1;
  } else if (utf <= 0x07FF) {
    // 2-byte unicode
    out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
    return 2;
  } else if (utf <= 0xFFFF) {
    // 3-byte unicode
    out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
    return 3;
  } else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
    return 4;
  } else {
    // error - use replacement character
    out[0] = (char)0xEF;
    out[1] = (char)0xBF;
    out[2] = (char)0xBD;
    return 3;
  }
}

static a_char *parse_raw_str(struct parser *parser, s_char raw_str) {
  // The parsed string will be at most as long as the raw string.
  // (\n -> one char)
  char buffer[raw_str.len];
  size_t buf_end = 0;

  // Skip the first and last bytes of the string.
  // These are the opening/closing quotes, doublequotes, or brackets (For
  // interpolation).
  for (size_t i = 1; i < raw_str.len - 1; i++) {
    int8_t c = raw_str.data[i];

    if (c == '\\') {

      switch (raw_str.data[i + 1]) {

      case 'r':
        buffer[buf_end++] = '\r';
        break;
      case 'n':
        buffer[buf_end++] = '\n';
        break;
      case 't':
        buffer[buf_end++] = '\t';
        break;
      case '{':
        buffer[buf_end++] = '{';
        break;
      case '"':
        buffer[buf_end++] = '"';
        break;
      case '\'':
        buffer[buf_end++] = '\'';
        break;
        break;
      case 'u':
        i += 2;

        if (raw_str.data[i] != '[') {
          return nullptr;
        }

        i++;

        uint8_t cpl = 0;
        char codepoint[8] = {0};

        while (raw_str.data[i] != ']') {

          if (cpl == 7)
            return nullptr;

          codepoint[cpl++] = raw_str.data[i++];
        }

        i++;

        long cp = strtol(codepoint, nullptr, 16);

        int result = encode_codepoint(buffer + buf_end, cp);

        buf_end += result;

        break;
      case '\\':
        buffer[buf_end++] = '\\';
        break;
      default:
        buffer[buf_end++] = c;
        continue;
      }

      i++;

    } else {
      buffer[buf_end++] = c;
    }
  }

  return a_char_create(buffer, buf_end);
};

static gab_value trim_prev_id(struct gab_triple gab, struct parser *parser) {
  s_char s = prev_src(parser);

  s.data++;
  s.len--;

  // These can cause collections during compilation.
  return gab_nstring(gab, s.len, s.data);
}

static inline bool match_token(struct parser *parser, gab_token tok) {
  return v_gab_token_val_at(&parser->src->tokens, parser->offset) == tok;
}

static inline bool match_terminator(struct parser *parser) {
  return match_token(parser, TOKEN_END) || match_token(parser, TOKEN_EOF);
}

static int vparser_error(struct gab_triple gab, struct parser *parser,
                         enum gab_status e, const char *fmt, va_list args) {
  gab_vfpanic(gab, stderr, args,
              (struct gab_err_argt){
                  .src = parser->src,
                  .message = gab_nil,
                  .status = e,
                  .tok = parser->offset - 1,
                  .note_fmt = fmt,
              });

  va_end(args);

  return 0;
}

static int parser_error(struct gab_triple gab, struct parser *parser,
                        enum gab_status e, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  return vparser_error(gab, parser, e, fmt, args);
}

static int eat_token(struct gab_triple gab, struct parser *parser) {
  if (match_token(parser, TOKEN_EOF))
    return parser_error(gab, parser, GAB_UNEXPECTED_EOF, "");

  parser->offset++;

  if (match_token(parser, TOKEN_ERROR)) {
    eat_token(gab, parser);
    return parser_error(gab, parser, GAB_MALFORMED_TOKEN,
                        "This token is malformed or unrecognized.");
  }

  return 1;
}

static inline int match_and_eat_token_of(struct gab_triple gab,
                                         struct parser *parser, size_t len,
                                         gab_token tok[len]) {
  for (size_t i = 0; i < len; i++)
    if (match_token(parser, tok[i]))
      return eat_token(gab, parser);

  return 0;
}

#define match_and_eat_token(gab, parser, ...)                                  \
  ({                                                                           \
    gab_token toks[] = {__VA_ARGS__};                                          \
    match_and_eat_token_of(gab, parser, sizeof(toks) / sizeof(gab_token),      \
                           toks);                                              \
  })

static inline int expect_token(struct gab_triple gab, struct parser *parser,
                               gab_token tok) {
  if (!match_token(parser, tok))
    return eat_token(gab, parser),
           parser_error(gab, parser, GAB_UNEXPECTED_TOKEN, FMT_UNEXPECTEDTOKEN,
                        tok_id(gab, tok));

  return eat_token(gab, parser);
}

gab_value parse_expression(struct gab_triple gab, struct parser *parser,
                           enum prec_k prec);

static inline void skip_newlines(struct gab_triple gab, struct parser *parser) {
  while (match_and_eat_token(gab, parser, TOKEN_NEWLINE))
    ;
}

bool msg_is_specialform(struct gab_triple gab, gab_value msg) {
  if (msg == gab_message(gab, "gab.runtime.env.put!"))
    return true;

  if (msg == gab_message(gab, "gab.runtime.block"))
    return true;

  return false;
}

gab_value node_value(struct gab_triple gab, gab_value node) {
  return gab_list(gab, 1, &node);
}

gab_value node_empty(struct gab_triple gab) { return gab_listof(gab); }

bool node_isempty(gab_value node) {
  return gab_valkind(node) == kGAB_RECORD && gab_reclen(node) == 0;
}

bool node_ismulti(struct gab_triple gab, gab_value node) {
  if (gab_valkind(node) != kGAB_RECORD)
    return false;

  switch (gab_valkind(gab_recshp(node))) {
  case kGAB_SHAPE:
    if (msg_is_specialform(gab, gab_mrecat(gab, node, "gab.msg")))
      return false;
    else
      return 1;
  case kGAB_SHAPELIST: {
    size_t len = gab_reclen(node);

    if (len == 0)
      return false;

    gab_value last_node = gab_uvrecat(node, len - 1);
    return node_ismulti(gab, last_node);
  }
  default:
    assert(false && "UNREACHABLE");
  }
}

size_t node_len(struct gab_triple gab, gab_value node);

gab_value node_tuple_lastnode(gab_value node) {
  assert(gab_valkind(node) == kGAB_RECORD);
  assert(gab_valkind(gab_recshp(node)) == kGAB_SHAPELIST);
  size_t len = gab_reclen(node);
  return gab_uvrecat(node, len - 1);
}

size_t node_valuelen(struct gab_triple gab, gab_value node) {
  // If this value node is a block, get the
  // last tuple in the block and return that
  // tuple's len
  if (gab_valkind(node) == kGAB_RECORD) {
    if (gab_valkind(gab_recshp(node)) == kGAB_SHAPELIST)
      return node_len(gab, node_tuple_lastnode(node));

    gab_value msg = gab_mrecat(gab, node, "gab.msg");

    if (msg_is_specialform(gab, msg))
      return 1;
  }

  // Otherwise, values are 1 long
  return 1;
}

size_t node_len(struct gab_triple gab, gab_value node) {
  assert(gab_valkind(node) == kGAB_RECORD);
  assert(gab_valkind(gab_recshp(node)) == kGAB_SHAPELIST);

  bool is_multi = node_ismulti(gab, node);
  size_t len = gab_reclen(node);
  size_t total_len = 0;

  // Tuple's length are the sum of their children
  // If the tuple as a whole is multi, subtract one
  // for that send
  for (size_t i = 0; i < len; i++)
    total_len += node_valuelen(gab, gab_uvrecat(node, i));

  if (total_len)
    return total_len - is_multi;
  else
    return 0;
}

gab_value node_send(struct gab_triple gab, gab_value lhs, gab_value msg,
                    gab_value rhs) {
  static const char *keys[] = {"gab.lhs", "gab.msg", "gab.rhs"};
  gab_value vals[] = {lhs, msg, rhs};
  return node_value(gab, gab_srecord(gab, 3, keys, vals));
}

gab_value wrap_in_trim(struct gab_triple gab, gab_value node, uint8_t want) {
  // Wrap the given node in a send to \gab.runtime.trim
  return node;
}

static gab_value parse_expressions_body(struct gab_triple gab,
                                        struct parser *parser) {

  skip_newlines(gab, parser);

  gab_value result = node_empty(gab);

  while (!match_terminator(parser) && !match_token(parser, TOKEN_EOF)) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    result = gab_lstcat(gab, result, node_value(gab, rhs));

    skip_newlines(gab, parser);
  }

  return node_value(gab, result);
}

gab_value parse_expressions(struct gab_triple gab, struct parser *parser,
                            gab_value lhs) {
  uint64_t line = prev_line(parser);

  gab_value res = parse_expressions_body(gab, parser);

  if (res == gab_undefined)
    return gab_undefined;

  if (match_token(parser, TOKEN_EOF))
    return parser_error(gab, parser, GAB_MISSING_END,
                        "Make sure the block at line $ is closed.",
                        gab_number(line)),
           gab_undefined;

  return res;
}

gab_value parse_expression(struct gab_triple gab, struct parser *parser,
                           enum prec_k prec) {
  if (!eat_token(gab, parser))
    return gab_undefined;

  size_t tok = prev_tok(parser);

  struct parse_rule rule = get_parse_rule(tok);

  if (rule.prefix == nullptr)
    return parser_error(gab, parser, GAB_UNEXPECTED_TOKEN,
                        FMT_EXPECTED_EXPRESSION),
           gab_undefined;

  gab_value have = rule.prefix(gab, parser, gab_undefined);

  while (prec <= get_parse_rule(curr_tok(parser)).prec) {
    if (have == gab_undefined)
      return gab_undefined;

    if (!eat_token(gab, parser))
      return gab_undefined;

    rule = get_parse_rule(prev_tok(parser));

    if (rule.infix != nullptr)
      have = rule.infix(gab, parser, have);
  }

  return have;
}

static gab_value parse_optional_expression_prec(struct gab_triple gab,
                                                struct parser *parser,
                                                gab_value lhs,
                                                enum prec_k prec) {
  if (!curr_prefix(parser, prec))
    return node_empty(gab);

  return parse_expression(gab, parser, prec);
}

gab_value parse_exp_num(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  double num = strtod((char *)prev_src(parser).data, nullptr);
  return node_value(gab, gab_number(num));
}

gab_value parse_exp_msg(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  gab_value id = trim_prev_id(gab, parser);

  return node_value(gab, gab_strtomsg(id));
}

gab_value parse_exp_sym(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  gab_value id = prev_id(gab, parser);

  return node_value(gab, gab_strtosym(id));
}

gab_value parse_exp_sig(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  gab_value id = trim_prev_id(gab, parser);

  return node_value(gab, gab_strtosig(id));
}

gab_value parse_exp_str(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  a_char *parsed = parse_raw_str(parser, prev_src(parser));

  if (parsed == NULL)
    return parser_error(
               gab, parser, GAB_MALFORMED_STRING,
               "Single quoted strings can contain interpolations.\n"
               "\n   " GAB_GREEN "'answer is: { " GAB_YELLOW "42" GAB_GREEN
               " }'\n" GAB_RESET
               "\nBoth single and double quoted strings can contain escape "
               "sequences.\n"
               "\n   " GAB_GREEN "'a newline -> " GAB_MAGENTA "\\n" GAB_GREEN
               ", or a forward slash -> " GAB_MAGENTA "\\\\" GAB_GREEN
               "'" GAB_RESET "\n   " GAB_GREEN
               "\"arbitrary unicode: " GAB_MAGENTA "\\u[" GAB_YELLOW
               "2502" GAB_MAGENTA "]" GAB_GREEN "\"" GAB_RESET),
           gab_undefined;

  gab_value str = gab_nstring(gab, parsed->len, parsed->data);

  a_char_destroy(parsed);

  return node_value(gab, str);
}

gab_value parse_exp_rec(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {

  gab_value result = node_empty(gab);

  if (match_and_eat_token(gab, parser, TOKEN_RBRACK))
    goto fin;

  for (;;) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    result = gab_lstcat(gab, result, rhs);

    skip_newlines(gab, parser);

    result = gab_lstcat(gab, result, rhs);

    skip_newlines(gab, parser);

    if (match_token(parser, TOKEN_RBRACK))
      break;
  }

  if (expect_token(gab, parser, TOKEN_RBRACK) < 0)
    return gab_undefined;

fin:
  return node_send(gab, node_value(gab, gab_sigil(gab, "gab.record")),
                   gab_message(gab, mGAB_CALL), result);
}

gab_value parse_exp_lst(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  gab_value result = node_empty(gab);

  if (match_and_eat_token(gab, parser, TOKEN_RBRACE))
    goto fin;

  for (;;) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    result = gab_lstcat(gab, result, rhs);

    skip_newlines(gab, parser);

    if (match_token(parser, TOKEN_RBRACE))
      break;
  }

  if (expect_token(gab, parser, TOKEN_RBRACE) < 0)
    return gab_undefined;

fin:
  return node_send(gab, node_value(gab, gab_sigil(gab, "gab.list")),
                   gab_message(gab, mGAB_CALL), result);
}

gab_value parse_exp_tup(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  skip_newlines(gab, parser);

  gab_value result = node_empty(gab);

  if (match_and_eat_token(gab, parser, TOKEN_RPAREN))
    goto fin;

  for (;;) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    // Problem - constant-nodes and send-nodes are both represented as records.
    // This concat just combines send nodes. OOps!
    result = gab_lstcat(gab, result, rhs);

    skip_newlines(gab, parser);

    if (match_token(parser, TOKEN_RPAREN))
      break;
  }

  if (expect_token(gab, parser, TOKEN_RPAREN) < 0)
    return gab_undefined;

fin:
  return result;
}

gab_value parse_exp_blk(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {

  gab_value result = parse_expressions_body(gab, parser);

  if (expect_token(gab, parser, TOKEN_END) < 0)
    return gab_undefined;

  return result;
}

gab_value parse_exp_send(struct gab_triple gab, struct parser *parser,
                         gab_value lhs) {
  gab_value msg = trim_prev_id(gab, parser);

  gab_value rhs =
      parse_optional_expression_prec(gab, parser, lhs, kBINARY_SEND);

  if (rhs == gab_undefined)
    return gab_undefined;

  return node_send(gab, lhs, gab_strtomsg(msg), rhs);
}

gab_value parse_exp_send_op(struct gab_triple gab, struct parser *parser,
                            gab_value lhs) {
  gab_value msg = prev_id(gab, parser);

  gab_value rhs =
      parse_optional_expression_prec(gab, parser, lhs, kBINARY_SEND);

  if (rhs == gab_undefined)
    return gab_undefined;

  return node_send(gab, lhs, gab_strtomsg(msg), rhs);
}

const struct parse_rule parse_rules[] = {
    {parse_exp_blk, nullptr, kNONE},     // DO
    {nullptr, nullptr, kNONE},           // END
    {parse_exp_lst, nullptr, kNONE},     // LBRACE
    {nullptr, nullptr, kNONE},           // RBRACE
    {parse_exp_rec, nullptr, kNONE},     // LBRACK
    {nullptr, nullptr, kNONE},           // RBRACK
    {parse_exp_tup, nullptr, kNONE},     // LPAREN
    {nullptr, nullptr, kNONE},           // RPAREN
    {nullptr, parse_exp_send, kSEND},    // SEND
    {nullptr, parse_exp_send_op, kSEND}, // OPERATOR
    {parse_exp_sym, nullptr, kNONE},     // SYMBOL
    {parse_exp_sig, nullptr, kNONE},     // SIGIL
    {parse_exp_msg, nullptr, kNONE},     // MESSAGE
    {parse_exp_str, nullptr, kNONE},     // STRING
    {parse_exp_num, nullptr, kNONE},     // NUMBER
    {nullptr, nullptr, kNONE},           // NEWLINE
    {nullptr, nullptr, kNONE},           // EOF
    {nullptr, nullptr, kNONE},           // ERROR
};

struct parse_rule get_parse_rule(gab_token k) { return parse_rules[k]; }

gab_value parse(struct gab_triple gab, struct parser *parser,
                uint8_t narguments, gab_value arguments[narguments]) {
  if (curr_tok(parser) == TOKEN_EOF)
    return gab_undefined;

  if (curr_tok(parser) == TOKEN_ERROR) {
    eat_token(gab, parser);
    parser_error(gab, parser, GAB_MALFORMED_TOKEN,
                 "This token is malformed or unrecognized.");
    return gab_undefined;
  }

  gab_value result = parse_expressions_body(gab, parser);

  if (result == gab_undefined)
    return gab_undefined;

  if (gab.flags & fGAB_AST_DUMP)
    gab_fvalinspect(stdout, result, -1), printf("\n");

  gab_iref(gab, result);
  gab_egkeep(gab.eg, result);

  return result;
}

gab_value gab_parse(struct gab_triple gab, struct gab_build_argt args) {
  gab.flags = args.flags;

  args.name = args.name ? args.name : "__main__";

  gab_gclock(gab);

  gab_value name = gab_string(gab, args.name);

  struct gab_src *src =
      gab_src(gab, name, (char *)args.source, strlen(args.source) + 1);

  struct parser parser = {.src = src};

  gab_value vargv[args.len];

  for (int i = 0; i < args.len; i++)
    vargv[i] = gab_string(gab, args.argv[i]);

  gab_value ast = parse(gab, &parser, args.len, vargv);

  gab_gcunlock(gab);

  return ast;
}

/*

 *************
 * COMPILING *
 *************

 Emiting bytecode for the gab AST.

[0] = LOAD_CONSTANT 0
[0, 1] = LOAD_NCONSTANT 0, 1

*/

struct bc {
  v_uint8_t bc;
  v_uint64_t bc_toks;
  v_gab_value *ks;

  struct gab_src *src;

  uint8_t prev_op, pprev_op;
  size_t prev_op_at;
};

static int vbc_error(struct gab_triple gab, struct bc *bc, enum gab_status e,
                     const char *fmt, va_list args) {
  gab_vfpanic(gab, stderr, args,
              (struct gab_err_argt){
                  .src = bc->src,
                  .message = gab_nil,
                  .status = e,
                  /*.tok = bc->offset - 1,*/
                  .note_fmt = fmt,
              });

  va_end(args);

  return 0;
}

static int bc_error(struct gab_triple gab, struct bc *bc, enum gab_status e,
                    const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  return vbc_error(gab, bc, e, fmt, args);
}

static inline void push_op(struct bc *bc, uint8_t op, size_t t) {
  bc->pprev_op = bc->prev_op;
  bc->prev_op = op;

  bc->prev_op_at = v_uint8_t_push(&bc->bc, op);
  v_uint64_t_push(&bc->bc_toks, t);
}

static inline void push_byte(struct bc *bc, uint8_t data, size_t t) {
  v_uint8_t_push(&bc->bc, data);
  v_uint64_t_push(&bc->bc_toks, t);
}

static inline void push_short(struct bc *bc, uint16_t data, size_t t) {
  push_byte(bc, (data >> 8) & 0xff, t);
  push_byte(bc, data & 0xff, t);
}

static inline uint16_t addk(struct gab_triple gab, struct bc *bc,
                            gab_value value) {
  gab_iref(gab, value);
  gab_egkeep(gab.eg, value);

  assert(bc->ks->len < UINT16_MAX);

  return v_gab_value_push(bc->ks, value);
}

static inline void push_k(struct bc *bc, uint16_t k, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  switch (bc->prev_op) {
  case OP_CONSTANT: {
    size_t prev_local_arg = bc->prev_op_at + 1;

    uint8_t prev_ka = v_uint8_t_val_at(&bc->bc, prev_local_arg);
    uint8_t prev_kb = v_uint8_t_val_at(&bc->bc, prev_local_arg + 1);

    uint16_t prev_k = prev_ka << 8 | prev_kb;

    v_uint8_t_pop(&bc->bc);
    v_uint8_t_pop(&bc->bc);
    v_uint64_t_pop(&bc->bc_toks);
    v_uint64_t_pop(&bc->bc_toks);

    bc->prev_op = OP_NCONSTANT;
    v_uint8_t_set(&bc->bc, bc->prev_op_at, OP_NCONSTANT);

    push_byte(bc, 2, t);
    push_short(bc, prev_k, t);
    push_short(bc, k, t);

    return;
  }
  case OP_NCONSTANT: {
    size_t prev_local_arg = bc->prev_op_at + 1;
    uint8_t prev_n = v_uint8_t_val_at(&bc->bc, prev_local_arg);
    v_uint8_t_set(&bc->bc, prev_local_arg, prev_n + 1);
    push_short(bc, k, t);
    return;
  }
  }
#endif

  push_op(bc, OP_CONSTANT, t);
  push_short(bc, k, t);
}

static inline void push_loadi(struct bc *bc, gab_value i, size_t t) {
  assert(i == gab_undefined || i == gab_true || i == gab_false || i == gab_nil);

  switch (i) {
  case gab_undefined:
    push_k(bc, 0, t);
    break;
  case gab_nil:
    push_k(bc, 1, t);
    break;
  case gab_false:
    push_k(bc, 2, t);
    break;
  case gab_true:
    push_k(bc, 3, t);
    break;
  default:
    assert(false && "Invalid constant");
  }
};

static inline void push_loadni(struct bc *bc, gab_value v, int n, size_t t) {
  for (int i = 0; i < n; i++)
    push_loadi(bc, v, t);
}

static inline void push_loadk(struct gab_triple gab, struct bc *bc, gab_value k,
                              size_t t) {
  push_k(bc, addk(gab, bc, k), t);
}

static inline void push_loadl(struct bc *bc, uint8_t local, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  switch (bc->prev_op) {
  case OP_LOAD_LOCAL: {
    size_t prev_local_arg = bc->prev_op_at + 1;
    uint8_t prev_local = v_uint8_t_val_at(&bc->bc, prev_local_arg);
    push_byte(bc, prev_local, t);
    push_byte(bc, local, t);
    v_uint8_t_set(&bc->bc, prev_local_arg, 2);
    v_uint8_t_set(&bc->bc, bc->prev_op_at, OP_NLOAD_LOCAL);
    bc->prev_op = OP_NLOAD_LOCAL;
    return;
  }
  case OP_NLOAD_LOCAL: {
    size_t prev_local_arg = bc->prev_op_at + 1;
    uint8_t old_arg = v_uint8_t_val_at(&bc->bc, prev_local_arg);
    v_uint8_t_set(&bc->bc, prev_local_arg, old_arg + 1);
    push_byte(bc, local, t);
    return;
  }
  }
#endif

  push_op(bc, OP_LOAD_LOCAL, t);
  push_byte(bc, local, t);
  return;
}

static inline void push_loadu(struct bc *bc, uint8_t upv, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  switch (bc->prev_op) {
  case OP_LOAD_UPVALUE: {
    size_t prev_upv_arg = bc->prev_op_at + 1;
    uint8_t prev_upv = v_uint8_t_val_at(&bc->bc, prev_upv_arg);
    push_byte(bc, prev_upv, t);
    push_byte(bc, upv, t);
    v_uint8_t_set(&bc->bc, prev_upv_arg, 2);
    v_uint8_t_set(&bc->bc, bc->prev_op_at, OP_NLOAD_UPVALUE);
    bc->prev_op = OP_NLOAD_UPVALUE;
    return;
  }
  case OP_NLOAD_UPVALUE: {
    size_t prev_upv_arg = bc->prev_op_at + 1;
    uint8_t old_arg = v_uint8_t_val_at(&bc->bc, prev_upv_arg);
    v_uint8_t_set(&bc->bc, prev_upv_arg, old_arg + 1);
    push_byte(bc, upv, t);
    return;
  }
  }
#endif

  push_op(bc, OP_LOAD_UPVALUE, t);
  push_byte(bc, upv, t);
  return;
}

static inline void push_storel(struct bc *bc, uint8_t local, size_t t) {
#if cGAB_SUPERINSTRUCTIONS
  switch (bc->prev_op) {
  case OP_POPSTORE_LOCAL: {
    size_t prev_local_arg = bc->prev_op_at + 1;
    uint8_t prev_local = v_uint8_t_val_at(&bc->bc, prev_local_arg);
    push_byte(bc, prev_local, t);
    push_byte(bc, local, t);
    v_uint8_t_set(&bc->bc, prev_local_arg, 2);
    v_uint8_t_set(&bc->bc, bc->prev_op_at, OP_NPOPSTORE_STORE_LOCAL);
    bc->prev_op = OP_NPOPSTORE_STORE_LOCAL;
    return;
  }
  case OP_NPOPSTORE_LOCAL: {
    size_t prev_loc_arg = bc->prev_op_at + 1;
    uint8_t old_arg = v_uint8_t_val_at(&bc->bc, prev_loc_arg);
    v_uint8_t_set(&bc->bc, prev_loc_arg, old_arg + 1);

    push_byte(bc, local, t);

    v_uint8_t_set(&bc->bc, bc->prev_op_at, OP_NPOPSTORE_STORE_LOCAL);
    bc->prev_op = OP_NPOPSTORE_STORE_LOCAL;
    return;
  }
  }
#endif

  push_op(bc, OP_STORE_LOCAL, t);
  push_byte(bc, local, t);
  return;
}

static inline uint8_t encode_arity(struct gab_triple gab, gab_value lhs,
                                   gab_value rhs) {
  if (rhs == gab_undefined || node_isempty(rhs)) {
    bool is_multi = node_ismulti(gab, lhs);
    size_t len = node_len(gab, lhs);
    assert(len < 64);
    return ((uint8_t)len << 2) | is_multi;
  }

  bool is_multi = node_ismulti(gab, rhs);
  size_t len = node_len(gab, rhs) + 1; // The trimmed lhs
  assert(len < 64);
  return ((uint8_t)len << 2) | is_multi;
}

static inline void push_send(struct gab_triple gab, struct bc *bc, gab_value m,
                             gab_value lhs, gab_value rhs, size_t t) {
  if (gab_valkind(m) == kGAB_STRING)
    m = gab_strtomsg(m);

  assert(gab_valkind(m) == kGAB_MESSAGE);

  uint16_t ks = addk(gab, bc, m);
  addk(gab, bc, gab_undefined);

  for (int i = 0; i < cGAB_SEND_CACHE_LEN; i++) {
    for (int j = 0; j < GAB_SEND_CACHE_SIZE; j++) {
      addk(gab, bc, gab_undefined);
    }
  }

  push_op(bc, OP_SEND, t);
  push_short(bc, ks, t);
  push_byte(bc, encode_arity(gab, lhs, rhs), t);
}

static inline void push_trim(struct bc *bc, uint8_t want, size_t t) {
  push_op(bc, OP_TRIM, t);
  push_byte(bc, want, t);
}

static inline void push_listpack(struct gab_triple gab, struct bc *bc,
                                 gab_value rhs, uint8_t below, uint8_t above,
                                 size_t t) {
  push_op(bc, OP_PACK, t);
  push_byte(bc, encode_arity(gab, rhs, gab_undefined), 0);
  push_byte(bc, below, t);
  push_byte(bc, above, t);
}

static inline void push_pop(struct bc *bc, uint8_t n, size_t t) {
  if (n > 1) {
    push_op(bc, OP_POP_N, t);
    push_byte(bc, n, t);
    return;
  }

#if cGAB_SUPERINSTRUCTIONS
  switch (bc->prev_op) {
  case OP_STORE_LOCAL:
    bc->prev_op_at = bc->bc.len - 2;
    bc->bc.data[bc->prev_op_at] = OP_POPSTORE_LOCAL;
    bc->prev_op = OP_POPSTORE_LOCAL;
    return;

  case OP_NPOPSTORE_STORE_LOCAL:
    bc->bc.data[bc->prev_op_at] = OP_NPOPSTORE_LOCAL;
    bc->prev_op = OP_NPOPSTORE_LOCAL;
    return;

  default:
    break;
  }
#endif

  push_op(bc, OP_POP, t);
}

static inline void push_ret(struct gab_triple gab, struct bc *bc, gab_value tup,
                            size_t t) {
  assert(node_len(gab, tup) < 16);

#if cGAB_TAILCALL
  if (node_len(gab, tup) == 0) {
    switch (bc->prev_op) {
    case OP_SEND: {
      uint8_t have_byte = v_uint8_t_val_at(&bc->bc, bc->bc.len - 1);
      v_uint8_t_set(&bc->bc, bc->bc.len - 1, have_byte | fHAVE_TAIL);
      push_op(bc, OP_RETURN, t);
      push_byte(bc, encode_arity(gab, tup, gab_undefined), t);
      return;
    }
    case OP_TRIM: {
      if (bc->pprev_op != OP_SEND)
        break;

      uint8_t have_byte = v_uint8_t_val_at(&bc->bc, bc->bc.len - 3);
      v_uint8_t_set(&bc->bc, bc->bc.len - 3, have_byte | fHAVE_TAIL);
      bc->prev_op = bc->pprev_op;
      bc->bc.len -= 2;
      bc->bc_toks.len -= 2;
      push_op(bc, OP_RETURN, t);
      push_byte(bc, encode_arity(gab, tup, gab_undefined), t);
      return;
    }
    }
  }
#endif

  push_op(bc, OP_RETURN, t);
  push_byte(bc, encode_arity(gab, tup, gab_undefined), t);
}

size_t locals_in_env(gab_value env) {
  size_t n = 0, len = gab_reclen(env);

  for (size_t i = 0; i < len; i++) {
    gab_value num_or_nil = gab_uvrecat(env, i);

    if (num_or_nil == gab_nil)
      n++;
  }

  return n;
}

size_t upvalues_in_env(gab_value env) {
  size_t n = 0, len = gab_reclen(env);

  for (size_t i = 0; i < len; i++) {
    gab_value num_or_undef = gab_uvrecat(env, i);

    if (gab_valkind(num_or_undef) == kGAB_NUMBER)
      n++;
  }

  return n;
}

gab_value peek_env(gab_value env, int depth) {
  size_t nenv = gab_reclen(env);

  if (depth + 1 > nenv)
    return gab_undefined;

  return gab_uvrecat(env, nenv - depth - 1);
}

gab_value put_env(struct gab_triple gab, gab_value env, int depth,
                  gab_value new_ctx) {
  size_t nenv = gab_reclen(env);

  assert(depth + 1 <= nenv);
  return gab_urecput(gab, env, nenv - depth - 1, new_ctx);
}

struct lookup_res {
  gab_value env;

  enum {
    kLOOKUP_NONE,
    kLOOKUP_UPV,
    kLOOKUP_LOC,
  } k;

  int idx;
};

/*
 * Modify the given environment to capture the message 'id'.
 * If already captured, the environment is unchanged.
 */
static struct lookup_res add_upvalue(struct gab_triple gab, gab_value env,
                                     gab_value id, int depth) {
  gab_value ctx = peek_env(env, depth);

  if (ctx == gab_undefined)
    return (struct lookup_res){env, kLOOKUP_NONE};

  // Don't pull redundant upvalues
  gab_value current_upv_idx = gab_recat(ctx, id);

  if (current_upv_idx != gab_undefined)
    return (struct lookup_res){env, kLOOKUP_UPV, current_upv_idx};

  uint16_t count = upvalues_in_env(ctx);

  // Throw some sort of error here
  if (count == GAB_UPVALUE_MAX)
    return (struct lookup_res){env, kLOOKUP_NONE};

  /*compiler_error(*/
  /*    bc, GAB_TOO_MANY_UPVALUES,*/
  /*    "For arbitrary reasons, blocks cannot capture more than 255 "*/
  /*    "variables.");*/
  ctx = gab_recput(gab, ctx, id, gab_number(count));
  env = put_env(gab, env, depth, ctx);

  return (struct lookup_res){env, kLOOKUP_UPV, count};
}

static int add_local(struct gab_triple gab, gab_value env, gab_value id) {
  assert(gab_reclen(env) > 0);
  size_t local_ctx = gab_reclen(env) - 1;
  gab_value ctx = gab_uvrecat(env, local_ctx);

  ctx = gab_recput(gab, ctx, id, gab_nil);
  env = gab_recput(gab, env, local_ctx, ctx);

  return env;
}

/*
 * Find for an id in the env at depth.
 */
static int resolve_local(struct gab_triple gab, gab_value env, gab_value id,
                         uint8_t depth) {
  gab_value ctx = peek_env(env, depth);

  if (ctx == gab_undefined)
    return -1;

  size_t idx = 0, len = gab_reclen(ctx);

  for (size_t i = 0; i < len; i++) {
    gab_value k = gab_ukrecat(ctx, i);
    gab_value v = gab_uvrecat(ctx, i);

    // If v isn't nil, then this is an upvalue. Skip it.
    if (v != gab_nil)
      continue;

    if (k == id)
      return idx;

    idx++;
  }

  return -1;
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_UPVALUE_NOT_FOUND if no matching upvalue is found,
 * and otherwise the offset of the upvalue.
 */
static struct lookup_res resolve_upvalue(struct gab_triple gab, gab_value env,
                                         gab_value name, uint8_t depth) {
  size_t nenvs = gab_reclen(env);

  if (depth >= nenvs)
    return (struct lookup_res){env, kLOOKUP_NONE};

  int local = resolve_local(gab, env, name, depth + 1);

  if (local >= 0)
    return add_upvalue(gab, env, name, depth);

  struct lookup_res res = resolve_upvalue(gab, env, name, depth + 1);

  if (res.k) // This means we found either a local, or an upvalue
    return add_upvalue(gab, env, name, depth + 1);

  return (struct lookup_res){env, kLOOKUP_NONE};
}

/* Returns COMP_ERR if an error is encountered,
 * COMP_ID_NOT_FOUND if no matching local or upvalue is found,
 * COMP_RESOLVED_TO_LOCAL if the id was a local, and
 * COMP_RESOLVED_TO_UPVALUE if the id was an upvalue.
 */
static struct lookup_res resolve_id(struct gab_triple gab, struct bc *bc,
                                    gab_value env, gab_value id) {
  int idx = resolve_local(gab, env, id, 0);

  if (idx == -1)
    return resolve_upvalue(gab, env, id, 0);
  else
    return (struct lookup_res){env, kLOOKUP_LOC, idx};
}

gab_value unquote_symbol(struct gab_triple gab, struct bc *bc, gab_value node,
                         gab_value env) {
  struct lookup_res res = resolve_id(gab, bc, env, node);

  switch (res.k) {
  case kLOOKUP_LOC:
    push_loadl(bc, res.idx, 0);
    return res.env;
  case kLOOKUP_UPV:
    push_loadu(bc, res.idx, 0);
    return res.env;
  default:
    bc_error(gab, bc, GAB_UNBOUND_SYMBOL, "$ is unbound", node);
    return gab_undefined;
  }
};

gab_value unquote_tuple(struct gab_triple gab, struct bc *bc, gab_value node,
                        gab_value env);

gab_value unquote_record(struct gab_triple gab, struct bc *bc, gab_value node,
                         gab_value env);

gab_value unquote_value(struct gab_triple gab, struct bc *bc, gab_value node,
                        gab_value env) {

  switch (gab_valkind(node)) {
  case kGAB_SIGIL:
  case kGAB_NUMBER:
  case kGAB_STRING:
  case kGAB_MESSAGE:
    push_loadk(gab, bc, node, 0);
    return env;

  case kGAB_SYMBOL:
    return unquote_symbol(gab, bc, node, env);

  case kGAB_RECORD:
    return unquote_record(gab, bc, node, env);

  default:
    assert(false && "UN-UNQUOATABLE VALUE");
  }
}

gab_value unquote_block(struct gab_triple gab, struct bc *bc, gab_value node,
                        gab_value env) {
  gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
  assert(gab_reclen(lhs_node) == 1);

  gab_value proto = gab_uvrecat(lhs_node, 0);
  assert(gab_valkind(proto) == kGAB_PROTOTYPE);

  push_op(bc, OP_BLOCK, 0);
  push_short(bc, addk(gab, bc, proto), 0);

  return env;
}

gab_value unquote_envput(struct gab_triple gab, struct bc *bc, gab_value node,
                         gab_value env) {
  gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
  gab_value rhs_node = gab_mrecat(gab, node, "gab.rhs");

  env = unquote_tuple(gab, bc, rhs_node, env);

  if (env == gab_undefined)
    return gab_undefined;

  size_t ntargets = gab_reclen(lhs_node);

  gab_value ids[ntargets];
  int listpack_at_n = -1, recpack_at_n = -1;

  for (size_t i = 0; i < ntargets; i++) {
    gab_value id = gab_uvrecat(lhs_node, i);

    if (gab_valkind(id) != kGAB_SYMBOL)
      return gab_undefined;

    if (gab_sstrendswith(gab_symtostr(id), "[]", 0)) {
      // Update the symbol to not have []
      if (listpack_at_n >= 0 || recpack_at_n >= 0)
        return gab_undefined;

      listpack_at_n = i;
    }

    ids[i] = id;
  }

  if (listpack_at_n >= 0)
    push_listpack(gab, bc, rhs_node, listpack_at_n,
                  ntargets - listpack_at_n - 1, 0);

  for (size_t i = 0; i < ntargets; i++) {
    gab_value target = ids[i];

    switch (gab_valkind(target)) {
    case kGAB_SYMBOL: {
      struct lookup_res res = resolve_id(gab, bc, env, target);

      switch (res.k) {
      case kLOOKUP_LOC:
        push_storel(bc, res.idx, 0);
        break;
      case kLOOKUP_UPV:
        assert(false && "INVALID ASSIGNMENT TARGET");
        break;
      case kLOOKUP_NONE:
        push_storel(bc, add_local(gab, env, target), 0);
        break;
      }

      break;
    }
    default:
      assert(false && "INVALID ASSIGNMENT TARGET");
      break;
    }

    if (i + 1 < ntargets)
      push_pop(bc, 1, 0);
  }

  return env;
}

gab_value unquote_specialform(struct gab_triple gab, struct bc *bc,
                              gab_value node, gab_value env) {
  gab_value msg = gab_mrecat(gab, node, "gab.msg");

  if (msg == gab_message(gab, "gab.runtime.env.put!"))
    return unquote_envput(gab, bc, node, env);

  if (msg == gab_message(gab, "gab.runtime.block"))
    return unquote_block(gab, bc, node, env);

  assert(false && "UNHANDLED SPECIAL FORM");
};

gab_value unquote_record(struct gab_triple gab, struct bc *bc, gab_value node,
                         gab_value env) {
  // Unquoting a record can mean one of two things:
  //  - This is a block, and each of the membres need to be unquoted and
  //  trimmed, except for the last.
  //  - This is a send, and the send needs to be emitted.
  switch (gab_valkind(gab_recshp(node))) {
  case kGAB_SHAPE: {
    // We have a send node!
    gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
    gab_value rhs_node = gab_mrecat(gab, node, "gab.rhs");
    gab_value msg = gab_mrecat(gab, node, "gab.msg");

    if (msg_is_specialform(gab, msg))
      return unquote_specialform(gab, bc, node, env);

    env = unquote_tuple(gab, bc, lhs_node, env);

    if (env == gab_undefined)
      return gab_undefined;

    if (node_ismulti(gab, lhs_node) && !node_isempty(rhs_node))
      push_trim(bc, 1, 0);

    env = unquote_tuple(gab, bc, rhs_node, env);

    if (env == gab_undefined)
      return gab_undefined;

    push_send(gab, bc, msg, lhs_node, rhs_node, 0);
    break;
  }
  case kGAB_SHAPELIST: {
    size_t len = gab_reclen(node);
    size_t last_node = len - 1;

    for (size_t i = 0; i < len; i++) {
      gab_value child_node = gab_uvrecat(node, i);

      env = unquote_tuple(gab, bc, child_node, env);

      if (env == gab_undefined)
        return gab_undefined;

      if (i != last_node)
        push_pop(bc, 1, 0);
    }
    break;
  }
  default:
    assert(false && "INVALID SHAPE KIND");
  }

  return env;
}

gab_value unquote_tuple(struct gab_triple gab, struct bc *bc, gab_value node,
                        gab_value env) {
  size_t len = gab_reclen(node);

  for (size_t i = 0; i < len; i++) {
    env = unquote_value(gab, bc, gab_uvrecat(node, i), env);

    if (env == gab_undefined)
      return gab_undefined;
  }

  return env;
}

struct expand_res {
  gab_value node, env;
};

#define EXPAND_NODE(n) ((struct expand_res){(n), env})
#define EXPAND_NODE_ENV(n, env) ((struct expand_res){(n), env})

struct expand_res expand_record(struct gab_triple gab, gab_value parent_node,
                                size_t n, gab_value env, gab_value mod);

struct expand_res expand_value(struct gab_triple gab, gab_value parent_node,
                               size_t n, gab_value env, gab_value mod) {
  gab_value node = gab_uvrecat(parent_node, n);

  switch (gab_valkind(node)) {
  case kGAB_RECORD:
    return expand_record(gab, parent_node, n, env, mod);
  default:
    return EXPAND_NODE(parent_node);
  }
}

struct expand_res expand_tuple(struct gab_triple gab, gab_value node,
                               gab_value env, gab_value mod) {

  size_t len = gab_reclen(node);

  for (size_t i = 0; i < len; i++) {
    struct expand_res val = expand_value(gab, node, i, env, mod);

    if (val.node == gab_undefined)
      return EXPAND_NODE(gab_undefined);

    node = val.node;
    env = val.env;
  }

  return EXPAND_NODE_ENV(node, env);
}

struct expand_res expand_record(struct gab_triple gab, gab_value parent_node,
                                size_t n, gab_value env, gab_value mod) {
  gab_value node = gab_uvrecat(parent_node, n);

  switch (gab_valkind(gab_recshp(node))) {
  case kGAB_SHAPE: {
    // We have a send node!
    gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
    gab_value rhs_node = gab_mrecat(gab, node, "gab.rhs");
    gab_value msg = gab_mrecat(gab, node, "gab.msg");

    /*
     * When the gc worker is parsing:
     *  * The worker is locked and accumulates allocations in its lock buffer.
     */
    a_gab_value *result = gab_sendmacro(gab, (struct gab_send_argt){
                                                 .message = msg,
                                                 .len = 4,
                                                 .argv =
                                                     (gab_value[]){
                                                         lhs_node,
                                                         rhs_node,
                                                         env,
                                                         mod,
                                                     },
                                             });

    if (result == nullptr) {
      // If i'm not a macro to expand, try to expand my lhs and rhs.
      struct expand_res lhs_res = expand_tuple(gab, lhs_node, env, mod);
      struct expand_res rhs_res = expand_tuple(gab, rhs_node, lhs_res.env, mod);

      if (lhs_res.node == lhs_node && lhs_res.env == env)
        if (rhs_res.node == rhs_node && rhs_res.env == env)
          return EXPAND_NODE(parent_node);

      if (lhs_res.node != lhs_node)
        node = gab_recput(gab, node, gab_message(gab, "gab.lhs"), lhs_res.node);

      if (rhs_res.node != rhs_node)
        node = gab_recput(gab, node, gab_message(gab, "gab.rhs"), rhs_res.node);

      parent_node = gab_urecput(gab, parent_node, n, node);
      return EXPAND_NODE_ENV(parent_node, rhs_res.env);
    }

    gab_value ok = result->len > 0 ? result->data[0] : gab_nil;

    if (ok != gab_ok)
      return a_gab_value_destroy(result), EXPAND_NODE(gab_undefined);

    gab_value result_node = result->len > 1 ? result->data[1] : gab_nil;
    gab_value result_env = result->len > 2 ? result->data[2] : gab_nil;

    gab_negkeep(gab.eg, result->len - 1, result->data + 1);

    node = result_node == gab_nil ? node : result_node;
    env = result_env == gab_nil ? env : result_env;

    parent_node = gab_urecput(gab, parent_node, n, node);

    // Return the node and env pair here
    return a_gab_value_destroy(result), EXPAND_NODE_ENV(parent_node, env);
  }
  case kGAB_SHAPELIST: {
    size_t len = gab_reclen(node);

    gab_value result = node;

    for (size_t i = 0; i < len; i++) {
      gab_value in_node = gab_uvrecat(result, i);
      struct expand_res rhs = expand_tuple(gab, in_node, env, mod);

      if (rhs.node == gab_undefined)
        return EXPAND_NODE(gab_undefined);

      if (env != rhs.env)
        env = rhs.env;

      if (in_node != rhs.node)
        result = gab_urecput(gab, result, i, rhs.node);
    }

    if (node != result)
      parent_node = gab_urecput(gab, parent_node, n, result);

    return EXPAND_NODE_ENV(parent_node, env);
  }
  default:
    assert(false && "INVALID SHAPE KIND");
  }

  return EXPAND_NODE(gab_undefined);
}

/*
 * Expand all the macros in the ast
 */
struct expand_res gab_expand(struct gab_triple gab, gab_value ast,
                             gab_value env, gab_value mod) {
  assert(gab_valkind(ast) == kGAB_RECORD);
  assert(gab_valkind(env) == kGAB_RECORD);

  struct expand_res r = {ast, env};

  for (;;) {
    struct expand_res r_in = r;
    r = expand_tuple(gab, r_in.node, r_in.env, mod);

    if (r.node == r_in.node && r.env == r_in.env)
      break;
  }

  return r;
}

void build_upvdata(gab_value env, uint8_t len, char data[static len]) {
  /*
   * Iterate through the env, and build out the data argument expected
   * by prototypes.
   *
   * I need to iterate through each environment in the stack
   *
   * If we're local, flag the captures as local.
   * Otherwise, do nothing.
   *
   * Upvalues are all the non-nil values in the context.
   *
   * As insertion-order is preserved, we can iterate the
   * ctx 0..len and be fine.
   *
   */
  size_t nenvs = gab_reclen(env);
  size_t ctx = gab_uvrecat(env, nenvs - 1);
  size_t nbindings = gab_reclen(ctx);
  size_t nth_upvalue = 0;

  for (size_t i = 0; i < nbindings; i++) {
    gab_value v = gab_uvrecat(ctx, i);

    if (v == gab_nil)
      continue;

    size_t idx = gab_valton(v);
    assert(idx < len);

    bool is_local = true;

    data[nth_upvalue++] = (idx << 1) | is_local;
  }
}

/*
 *    *******
 *    * ENV *
 *    *******
 *
 *    The ENV argument is a stack of records.
 *
 *    [ { self {}, a {} }, { self {}, b {} } ]
 *
 *    Each variable has a record of attributes:
 *      * captured? -> number - is the variable captured by a child scope? If
 * so, whats its upv index?
 *
 *    Each scope needs to keep track of:
 *     - local variables introduced in this scope
 *     - local variables captured by child scopes
 *     - update parent scopes whose variables it captures
 */
union gab_value_pair gab_unquote(struct gab_triple gab, gab_value ast,
                                 gab_value env, gab_value mod) {
  assert(gab_valkind(ast) == kGAB_RECORD);
  assert(gab_valkind(env) == kGAB_RECORD);

  size_t nenvs = gab_reclen(env);
  assert(nenvs > 0);

  struct gab_src *src = d_gab_src_read(&gab.eg->sources, mod);

  if (src == nullptr)
    return (union gab_value_pair){{gab_undefined, gab_undefined}};

  struct expand_res res = gab_expand(gab, ast, env, mod);

  if (res.node == gab_undefined)
    return (union gab_value_pair){{gab_undefined, gab_undefined}};

  struct bc bc = {.ks = &src->constants, .src = src};

  size_t nargs = gab_reclen(gab_uvrecat(env, nenvs - 1));
  assert(nargs < GAB_ARG_MAX);

  push_trim(&bc, 0, 0); // A trim to be patched later.
  assert(bc.bc.len == bc.bc_toks.len);

  env = unquote_tuple(gab, &bc, res.node, res.env);
  assert(bc.bc.len == bc.bc_toks.len);

  if (env == gab_undefined)
    return (union gab_value_pair){{gab_undefined, gab_undefined}};

  assert(gab_reclen(env) == nenvs);

  gab_value local_env = gab_uvrecat(env, nenvs - 1);
  assert(bc.bc.len == bc.bc_toks.len);

  push_ret(gab, &bc, res.node, 0);

  size_t nlocals = locals_in_env(local_env);
  assert(nlocals < GAB_LOCAL_MAX);

  v_uint8_t_set(&bc.bc, 1, nlocals); // Patch the initial trim

  size_t nupvalues = upvalues_in_env(local_env);
  assert(nupvalues < GAB_UPVALUE_MAX);

  assert(bc.bc.len == bc.bc_toks.len);

  size_t len = bc.bc.len;
  size_t end = gab_srcappend(src, len, bc.bc.data, bc.bc_toks.data);
  v_uint8_t_destroy(&bc.bc);
  v_uint64_t_destroy(&bc.bc_toks);
  size_t begin = end - len;

  char data[nupvalues];

  build_upvdata(env, nupvalues, data);

  gab_value proto = gab_prototype(gab, src, begin, len,
                                  (struct gab_prototype_argt){
                                      .nupvalues = nupvalues,
                                      .nlocals = nlocals,
                                      .narguments = nargs,
                                      .nslots = 0,
                                      .data = data,
                                  });

  // call srcappend to append bytecode to src module
  // actually track bc_tok offset
  // addk should add constant to source
  // addk should check for common immediates
  //  - :ok, :err, :nil, :none, 1, 2

  return (union gab_value_pair){
      .environment = env,
      .prototype = proto,
  };
}

gab_value gab_build(struct gab_triple gab, struct gab_build_argt args) {
  gab.flags = args.flags;

  args.name = args.name ? args.name : "__main__";

  gab_gclock(gab);

  gab_value mod = gab_string(gab, args.name);

  gab_value ast = gab_parse(gab, args);

  if (ast == gab_undefined)
    return gab_gcunlock(gab), gab_undefined;

  struct gab_src *src = d_gab_src_read(&gab.eg->sources, mod);

  if (src == nullptr)
    return gab_gcunlock(gab), gab_undefined;

  gab_value vargv[args.len + 1];
  vargv[0] = gab_string(gab, "self");

  for (int i = 0; i < args.len; i++) {
    vargv[i + 1] = gab_string(gab, args.argv[i]);
  }

  gab_value env = gab_listof(
      gab, gab_shptorec(gab, gab_shape(gab, 1, args.len + 1, vargv)));

  union gab_value_pair res = gab_unquote(gab, ast, env, mod);

  if (res.prototype == gab_undefined)
    return gab_gcunlock(gab), gab_undefined;

  gab_srccomplete(gab, src);

  if (gab.flags & fGAB_BUILD_DUMP)
    gab_fmodinspect(stdout, GAB_VAL_TO_PROTOTYPE(res.prototype));

  gab_value main = gab_block(gab, res.prototype);

  gab_iref(gab, main);
  gab_egkeep(gab.eg, main);

  return gab_gcunlock(gab), main;
}
