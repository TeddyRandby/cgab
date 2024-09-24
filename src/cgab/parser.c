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
                        enum gab_status e, const char *fmt, ...);

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

gab_value node_value(struct gab_triple gab, gab_value node) {
  return gab_list(gab, 1, &node);
}

gab_value node_empty(struct gab_triple gab) {
  return gab_list(gab, 0, nullptr);
}

bool node_isempty(gab_value node) {
  return gab_valkind(node) == kGAB_RECORD && gab_reclen(node) == 0;
}

bool node_ismulti(gab_value node) {
  if (gab_valkind(node) != kGAB_RECORD)
    return false;

  switch (gab_valkind(gab_recshp(node))) {
  case kGAB_SHAPE:
    return true;
  case kGAB_SHAPELIST: {
    size_t len = gab_reclen(node);

    if (len == 0)
      return false;

    gab_value last_node = gab_uvrecat(node, len - 1);
    return node_ismulti(last_node);
  }
  default:
    assert(false && "UNREACHABLE");
  }
}

size_t node_len(gab_value node);

size_t node_valuelen(gab_value node) {
  // If this value node is a block, get the
  // last tuple in the block and return that
  // tuple's len
  if (gab_valkind(node) == kGAB_RECORD)
    if (gab_valkind(gab_recshp(node)) == kGAB_SHAPELIST)
      return node_len(gab_uvrecat(node, gab_reclen(node) - 1));

  // Otherwise, values are 1 long
  return 1;
}

size_t node_len(gab_value node) {
  assert(gab_valkind(node) == kGAB_RECORD);

  bool is_multi = node_ismulti(node);
  size_t len = gab_reclen(node);
  size_t total_len = 0;

  // Tuple's length are the sum of their children
  // If the tuple as a whole is multi, subtract one
  // for that send
  for (size_t i = 0; i < len; i++)
    total_len += node_valuelen(gab_uvrecat(node, i));

  return total_len - is_multi;
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

    result = gab_reccat(gab, result, node_value(gab, rhs));

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
  gab_value id = prev_id(gab, parser);

  return node_value(gab, gab_strtomsg(id));
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

  for (;;) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    result = gab_reccat(gab, result, rhs);

    skip_newlines(gab, parser);

    result = gab_reccat(gab, result, rhs);

    skip_newlines(gab, parser);

    if (match_token(parser, TOKEN_RBRACK))
      break;
  }

  if (expect_token(gab, parser, TOKEN_RBRACK) < 0)
    return gab_undefined;

  return node_send(gab, node_value(gab, gab_sigil(gab, "gab.record")),
                   gab_message(gab, "make"), result);
}

gab_value parse_exp_lst(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  gab_value result = node_empty(gab);

  for (;;) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    result = gab_reccat(gab, result, rhs);

    skip_newlines(gab, parser);

    if (match_token(parser, TOKEN_RBRACE))
      break;
  }

  if (expect_token(gab, parser, TOKEN_RBRACE) < 0)
    return gab_undefined;

  return node_send(gab, node_value(gab, gab_sigil(gab, "gab.list")),
                   gab_message(gab, "make"), result);
}

gab_value parse_exp_tup(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  skip_newlines(gab, parser);

  if (match_and_eat_token(gab, parser, TOKEN_RPAREN))
    return node_empty(gab);

  gab_value result = node_empty(gab);

  for (;;) {
    gab_value rhs = parse_expression(gab, parser, kASSIGNMENT);

    if (rhs == gab_undefined)
      return gab_undefined;

    // Problem - constant-nodes and send-nodes are both represented as records.
    // This concat just combines send nodes. OOps!
    result = gab_reccat(gab, result, rhs);

    skip_newlines(gab, parser);

    if (match_token(parser, TOKEN_RPAREN))
      break;
  }

  if (expect_token(gab, parser, TOKEN_RPAREN) < 0)
    return gab_undefined;

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

const struct parse_rule parse_rules[] = {
    {parse_exp_blk, nullptr, kNONE},  // DO
    {nullptr, nullptr, kNONE},        // END
    {nullptr, nullptr, kNONE},        // DOT
    {nullptr, nullptr, kNONE},        // COMMA
    {nullptr, nullptr, kNONE},        // COLON
    {nullptr, nullptr, kNONE},        // COLON_COLON
    {parse_exp_lst, nullptr, kNONE},  // LBRACE
    {nullptr, nullptr, kNONE},        // RBRACE
    {parse_exp_rec, nullptr, kNONE},  // LBRACK
    {nullptr, nullptr, kNONE},        // RBRACK
    {parse_exp_tup, nullptr, kNONE},  // LPAREN
    {nullptr, nullptr, kNONE},        // RPAREN
    {nullptr, parse_exp_send, kSEND}, // SEND
    {parse_exp_msg, nullptr, kNONE},  // OPERATOR
    {parse_exp_msg, nullptr, kNONE},  // IDENTIFIER
    {parse_exp_sig, nullptr, kNONE},  // SIGIL
    {parse_exp_str, nullptr, kNONE},  // STRING
    {nullptr, nullptr, kNONE},        // INTERPOLATION END
    {nullptr, nullptr, kNONE},        // INTERPOLATION MIDDLE
    {nullptr, nullptr, kNONE},        // INTERPOLATION END
    {parse_exp_num, nullptr, kNONE},  // NUMBER
    {nullptr, nullptr, kNONE},        // MESSAGE
    {nullptr, nullptr, kNONE},        // NEWLINE
    {nullptr, nullptr, kNONE},        // EOF
    {nullptr, nullptr, kNONE},        // ERROR
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

  gab_value env = gab_listof(gab, gab_shapeof(gab, gab_message(gab, "self")));

  gab_unquote(gab, ast, env);

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
  v_gab_value ks;

  uint8_t prev_op;
  size_t prev_op_at;

  uint8_t ncontext;
  gab_value contexts[cGAB_FUNCTION_DEF_NESTING_MAX];
};

static inline void push_op(struct bc *bc, uint8_t op, size_t t) {
  bc->prev_op = op;

  bc->prev_op_at = v_uint8_t_push(&bc->bc, op);
}

static inline void push_byte(struct bc *bc, uint8_t data, size_t t) {
  v_uint8_t_push(&bc->bc, data);
}

static inline void push_short(struct bc *bc, uint16_t data, size_t t) {
  push_byte(bc, (data >> 8) & 0xff, t);
  push_byte(bc, data & 0xff, t);
}

static inline uint16_t addk(struct gab_triple gab, struct bc *bc,
                            gab_value value) {
  gab_iref(gab, value);
  gab_egkeep(gab.eg, value);

  assert(bc->ks.len < UINT16_MAX);

  return v_gab_value_push(&bc->ks, value);
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

static inline uint8_t encode_arity(gab_value lhs, gab_value rhs) {
  if (rhs == gab_undefined || node_isempty(rhs)) {
    bool is_multi = node_ismulti(lhs);
    size_t len = node_len(lhs);
    assert(len < 64);
    return ((uint8_t)len << 2) | is_multi;
  }

  bool is_multi = node_ismulti(rhs);
  size_t len = node_len(rhs) + 1; // The trimmed lhs
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
  push_byte(bc, encode_arity(lhs, rhs), t);
}

static inline void push_trim(struct bc *bc, uint8_t want, size_t t) {
  push_op(bc, OP_TRIM, t);
  push_byte(bc, want, t);
}

static inline void push_ret(struct bc *bc, gab_value tup, size_t t) {
  assert(gab_reclen(tup) < 16);

#if cGAB_TAILCALL
  if (gab_reclen(tup) == 0) {
    switch (bc->prev_op) {
    case OP_SEND: {
      uint8_t have_byte = v_uint8_t_val_at(&bc->bc, bc->bc.len - 1);
      v_uint8_t_set(&bc->bc, bc->bc.len - 1, have_byte | fHAVE_TAIL);
      push_op(bc, OP_RETURN, t);
      push_byte(bc, encode_arity(tup, gab_undefined), t);
      return;
    }
    case OP_TRIM: {
      /*if (bc->pprev_op != OP_SEND)*/
      /*  break;*/
      /*uint8_t have_byte = v_uint8_t_val_at(&bc->bc, bc->bc.len - 3);*/
      /*v_uint8_t_set(&bc->bc, bc->bc.len - 3, have_byte | bcHAVE_TAIL);*/
      /*bc->prev_op = bc->pprev_op;*/
      /*bc->bc.len -= 2;*/
      /*bc->bc_toks.len -= 2;*/
      /*rhs.status -= !rhs.multi;*/
      /*rhs.multi = true;*/
      /*push_op(bc, OP_RETURN, t);*/
      /*push_byte(bc, encode_arity(rhs), t);*/
      /*return;*/
    }
    }
  }
#endif

  push_op(bc, OP_RETURN, t);
  push_byte(bc, encode_arity(tup, gab_undefined), t);
}

struct var {
  enum : uint8_t {
    kVAR_LOCAL,
    kVAR_UPVAL,
    kVAR_NONE,
  } k;

  uint16_t idx;
};

struct var env_lookup(gab_value env, gab_value node) {
  assert(gab_valkind(env) == kGAB_RECORD);
  printf("%V LOOKUP %V\n", env, node);

  size_t len = gab_reclen(env);
  for (size_t i = 0; i < len; i++) {
    gab_value scope = gab_uvrecat(env, len - i - 1);

    printf("| %V LOOKUP %V\n", scope, node);
    if (gab_shphas(scope, node)) {
      printf("FOUND\n");
      return (struct var){
          i == 0 ? kVAR_LOCAL : kVAR_UPVAL,
          gab_shpfind(scope, node),
      };
    }

    printf("NOT FOUND\n");
  }

  return (struct var){kVAR_NONE};
};

gab_value unquote_message(struct gab_triple gab, struct bc *bc, gab_value node,
                          gab_value env) {
  struct var res = env_lookup(env, node);

  switch (res.k) {
  case kVAR_LOCAL:
    push_loadl(bc, res.idx, -1);
    break;
  case kVAR_UPVAL:
    push_loadu(bc, res.idx, -1);
    break;
  case kVAR_NONE:
    return gab_err;
  }

  return gab_ok;
};

void unquote_tuple(struct gab_triple gab, struct bc *bc, gab_value node,
                   gab_value env);

void unquote_record(struct gab_triple gab, struct bc *bc, gab_value node,
                    gab_value env);

void unquote_value(struct gab_triple gab, struct bc *bc, gab_value node,
                   gab_value env) {
  printf("UNQUOTE VALUE: %V\n", node);

  switch (gab_valkind(node)) {
  case kGAB_SIGIL:
  case kGAB_NUMBER:
  case kGAB_STRING:
    push_loadk(gab, bc, node, -1);
    break;

  case kGAB_MESSAGE:
    unquote_message(gab, bc, node, env);
    break;

  case kGAB_RECORD:
    unquote_record(gab, bc, node, env);
    break;

  default:
    assert(false && "UN-UNQUOATABLE VALUE");
  }
}

bool msg_is_specialform(struct gab_triple gab, gab_value msg) {
  if (msg == gab_message(gab, "gab.runtime.env.put!"))
    return true;

  return false;
}

void unquote_envput(struct gab_triple gab, struct bc *bc, gab_value node,
                    gab_value env) {
  gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
  gab_value rhs_node = gab_mrecat(gab, node, "gab.rhs");

  unquote_tuple(gab, bc, rhs_node, env);

  size_t ntargets = gab_reclen(lhs_node);
  for (size_t i = 0; i < ntargets; i++) {
    gab_value target = gab_uvrecat(lhs_node, ntargets - i - 1);
    switch (gab_valkind(target)) {
    case kGAB_MESSAGE: {
      struct var res = env_lookup(env, target);

      switch (res.k) {
      case kVAR_LOCAL:
        push_storel(bc, res.idx, -1);
        break;
      case kVAR_UPVAL:
      case kVAR_NONE:
        assert(false && "INVALID ASSIGNMENT TARGET");
        break;
      }

      break;
    }
    default:
      assert(false && "INVALID ASSIGNMENT TARGET");
      break;
    }
  }
}

void unquote_specialform(struct gab_triple gab, struct bc *bc, gab_value node,
                         gab_value env) {
  gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
  gab_value rhs_node = gab_mrecat(gab, node, "gab.rhs");
  gab_value msg = gab_mrecat(gab, node, "gab.msg");

  printf("UNQUOTE_SPECIALSEND: %V\n %V .%V %V\n", node, lhs_node, msg,
         rhs_node);

  if (msg == gab_message(gab, "gab.runtime.env.put!"))
    return unquote_envput(gab, bc, node, env);
};

void unquote_record(struct gab_triple gab, struct bc *bc, gab_value node,
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

    printf("UNQUOTE_SEND: %V\n %V .%V %V\n", node, lhs_node, msg, rhs_node);

    unquote_tuple(gab, bc, lhs_node, env);

    if (node_ismulti(lhs_node) && !node_isempty(rhs_node))
      push_trim(bc, 1, -1);

    unquote_tuple(gab, bc, rhs_node, env);

    printf("NODE: %V, LEN: %lu\n", lhs_node, node_len(lhs_node));

    push_send(gab, bc, msg, lhs_node, rhs_node, -1);
    break;
  }
  case kGAB_SHAPELIST: {
    printf("UNQUOTE_BLOCK: %V\n", node);
    size_t len = gab_reclen(node);
    size_t last_node = len - 1;

    for (size_t i = 0; i < len; i++) {
      gab_value child_node = gab_uvrecat(node, i);

      unquote_tuple(gab, bc, child_node, env);

      if (i != last_node)
        push_trim(bc, 0, -1);
    }
    break;
  }
  default:
    assert(false && "INVALID SHAPE KIND");
  }
}

void unquote_tuple(struct gab_triple gab, struct bc *bc, gab_value node,
                   gab_value env) {
  size_t len = gab_reclen(node);
  printf("UNQUOTE TUPLE: %V\n", node);
  for (size_t i = 0; i < len; i++)
    unquote_value(gab, bc, gab_uvrecat(node, i), env);
}

struct expand_res {
  gab_value node, env;
};

#define EXPAND_NODE(n) ((struct expand_res){(n), env})
#define EXPAND_NODE_ENV(n, env) ((struct expand_res){(n), env})

struct expand_res expand_record(struct gab_triple gab, gab_value node,
                                gab_value env);

struct expand_res expand_value(struct gab_triple gab, gab_value node,
                               gab_value env) {
  printf("EXPAND VALUE: %V\n", node);

  switch (gab_valkind(node)) {
  case kGAB_SIGIL:
  case kGAB_NUMBER:
  case kGAB_STRING:
  case kGAB_MESSAGE:
    return EXPAND_NODE(node_value(gab, node));

  case kGAB_RECORD:
    return expand_record(gab, node, env);

  default:
    assert(false && "UN-UNQUOATABLE VALUE");
  }
}

struct expand_res expand_tuple(struct gab_triple gab, gab_value node,
                               gab_value env) {
  printf("EXPAND TUPLE: %V\n", node);

  gab_value res = node_empty(gab);

  size_t len = gab_reclen(node);
  for (size_t i = 0; i < len; i++) {
    struct expand_res val = expand_value(gab, gab_uvrecat(node, i), env);

    if (val.node == gab_undefined)
      return EXPAND_NODE(gab_undefined);

    res = gab_reccat(gab, res, val.node);
    env = val.env;
  }

  return EXPAND_NODE_ENV(res, env);
}

struct expand_res expand_record(struct gab_triple gab, gab_value node,
                                gab_value env) {
  switch (gab_valkind(gab_recshp(node))) {
  case kGAB_SHAPE: {
    // We have a send node!
    gab_value lhs_node = gab_mrecat(gab, node, "gab.lhs");
    gab_value rhs_node = gab_mrecat(gab, node, "gab.rhs");
    gab_value msg = gab_mrecat(gab, node, "gab.msg");

    printf("EXPAND_SEND: %V\n %V .%V %V\n", node, lhs_node, msg, rhs_node);

    if (gab_reclen(lhs_node) == 0)
      return EXPAND_NODE(gab_undefined);

    gab_value receiver = gab_uvrecat(lhs_node, 0);

    a_gab_value *result = gab_sendmacro(gab, (struct gab_send_argt){
                                                 .receiver = receiver,
                                                 .message = msg,
                                                 .len = 3,
                                                 .argv =
                                                     (gab_value[]){
                                                         lhs_node,
                                                         rhs_node,
                                                         env,
                                                     },
                                             });

    if (result == nullptr)
      return EXPAND_NODE(node_value(gab, node));

    gab_value ok = result->len > 0 ? result->data[0] : gab_nil;

    if (ok != gab_ok)
      return EXPAND_NODE(gab_undefined);

    gab_value result_node = result->len > 1 ? result->data[1] : gab_nil;
    gab_value result_env = result->len > 2 ? result->data[2] : gab_nil;

    printf("MACRO RETURNED %V, %V\n", result_node, result_env);

    node = result_node == gab_nil ? node : result_node;
    env = result_env == gab_nil ? env : result_env;

    // Return the node and env pair here
    return EXPAND_NODE_ENV(node, env);
  }
  case kGAB_SHAPELIST: {
    printf("EXPAND_BLOCK: %V\n", node);
    size_t len = gab_reclen(node);

    gab_value result = node_empty(gab);

    for (size_t i = 0; i < len; i++) {
      struct expand_res rhs = expand_tuple(gab, gab_uvrecat(node, i), env);

      if (rhs.node == gab_undefined)
        return EXPAND_NODE(gab_undefined);

      result = gab_reccat(gab, result, node_value(gab, rhs.node));
      env = rhs.env;
    }

    return EXPAND_NODE_ENV(node_value(gab, result), env);
  }
  default:
    assert(false && "INVALID SHAPE KIND");
  }

  return EXPAND_NODE(gab_undefined);
}

static uint64_t dumpInstruction(FILE *stream, struct bc *self, uint64_t offset);

static uint64_t dumpSimpleInstruction(FILE *stream, struct bc *self,
                                      uint64_t offset) {
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];
  fprintf(stream, "%-25s\n", name);
  return offset + 1;
}

static uint64_t dumpSendInstruction(FILE *stream, struct bc *self,
                                    uint64_t offset) {
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];

  uint16_t constant = ((uint16_t)v_uint8_t_val_at(&self->bc, offset + 1)) << 8 |
                      v_uint8_t_val_at(&self->bc, offset + 2);

  gab_value msg = v_gab_value_val_at(&self->ks, constant);

  uint8_t have = v_uint8_t_val_at(&self->bc, offset + 3);

  uint8_t var = have & fHAVE_VAR;
  uint8_t tail = have & fHAVE_TAIL;
  have = have >> 2;

  fprintf(stream, "%-25s" GAB_BLUE, name);
  gab_fvalinspect(stream, msg, 0);
  fprintf(stream, GAB_RESET " (%d%s)%s\n", have, var ? " & more" : "",
          tail ? " [TAILCALL]" : "");

  return offset + 4;
}

static uint64_t dumpByteInstruction(FILE *stream, struct bc *self,
                                    uint64_t offset) {
  uint8_t operand = v_uint8_t_val_at(&self->bc, offset + 1);
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];
  fprintf(stream, "%-25s%hhx\n", name, operand);
  return offset + 2;
}

static uint64_t dumpTrimInstruction(FILE *stream, struct bc *self,
                                    uint64_t offset) {
  uint8_t wantbyte = v_uint8_t_val_at(&self->bc, offset + 1);
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];
  fprintf(stream, "%-25s%hhx\n", name, wantbyte);
  return offset + 2;
}

static uint64_t dumpReturnInstruction(FILE *stream, struct bc *self,
                                      uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->bc, offset + 1);
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s%hhx%s\n", "RETURN", have,
          havebyte & fHAVE_VAR ? " & more" : "");
  return offset + 2;
}

static uint64_t dumpPackInstruction(FILE *stream, struct bc *self,
                                    uint64_t offset) {
  uint8_t havebyte = v_uint8_t_val_at(&self->bc, offset + 1);
  uint8_t operandA = v_uint8_t_val_at(&self->bc, offset + 2);
  uint8_t operandB = v_uint8_t_val_at(&self->bc, offset + 3);
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];
  uint8_t have = havebyte >> 2;
  fprintf(stream, "%-25s(%hhx%s) -> %hhx %hhx\n", name, have,
          havebyte & fHAVE_VAR ? " & more" : "", operandA, operandB);
  return offset + 4;
}

static uint64_t dumpConstantInstruction(FILE *stream, struct bc *self,
                                        uint64_t offset) {
  uint16_t constant = ((uint16_t)v_uint8_t_val_at(&self->bc, offset + 1)) << 8 |
                      v_uint8_t_val_at(&self->bc, offset + 2);
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];
  fprintf(stream, "%-25s", name);
  gab_fvalinspect(stdout, v_gab_value_val_at(&self->ks, constant), 0);
  fprintf(stream, "\n");
  return offset + 3;
}

static uint64_t dumpNConstantInstruction(FILE *stream, struct bc *self,
                                         uint64_t offset) {
  const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];
  fprintf(stream, "%-25s", name);

  uint8_t n = v_uint8_t_val_at(&self->bc, offset + 1);

  for (int i = 0; i < n; i++) {
    uint16_t constant =
        ((uint16_t)v_uint8_t_val_at(&self->bc, offset + 2 + (2 * i))) << 8 |
        v_uint8_t_val_at(&self->bc, offset + 3 + (2 * i));

    gab_fvalinspect(stdout, v_gab_value_val_at(&self->ks, constant), 0);

    if (i < n - 1)
      fprintf(stream, ", ");
  }

  fprintf(stream, "\n");
  return offset + 2 + (2 * n);
}

static uint64_t dumpInstruction(FILE *stream, struct bc *self,
                                uint64_t offset) {
  uint8_t op = v_uint8_t_val_at(&self->bc, offset);
  switch (op) {
  case OP_POP:
  case OP_NOP:
    return dumpSimpleInstruction(stream, self, offset);
  case OP_PACK:
    return dumpPackInstruction(stream, self, offset);
  case OP_NCONSTANT:
    return dumpNConstantInstruction(stream, self, offset);
  case OP_CONSTANT:
    return dumpConstantInstruction(stream, self, offset);
  case OP_SEND:
  case OP_SEND_BLOCK:
  case OP_SEND_NATIVE:
  case OP_SEND_PROPERTY:
  case OP_SEND_PRIMITIVE_CONCAT:
  case OP_SEND_PRIMITIVE_SPLAT:
  case OP_SEND_PRIMITIVE_ADD:
  case OP_SEND_PRIMITIVE_SUB:
  case OP_SEND_PRIMITIVE_MUL:
  case OP_SEND_PRIMITIVE_DIV:
  case OP_SEND_PRIMITIVE_MOD:
  case OP_SEND_PRIMITIVE_EQ:
  case OP_SEND_PRIMITIVE_LT:
  case OP_SEND_PRIMITIVE_LTE:
  case OP_SEND_PRIMITIVE_GT:
  case OP_SEND_PRIMITIVE_GTE:
  case OP_SEND_PRIMITIVE_CALL_BLOCK:
  case OP_SEND_PRIMITIVE_CALL_NATIVE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_PRIMITIVE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_NATIVE:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_CONSTANT:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_BLOCK:
  case OP_TAILSEND_PRIMITIVE_CALL_MESSAGE_BLOCK:
  case OP_SEND_PRIMITIVE_CALL_MESSAGE_PROPERTY:
  case OP_TAILSEND_BLOCK:
  case OP_TAILSEND_PRIMITIVE_CALL_BLOCK:
  case OP_LOCALSEND_BLOCK:
  case OP_LOCALTAILSEND_BLOCK:
  case OP_MATCHSEND_BLOCK:
  case OP_MATCHTAILSEND_BLOCK:
    return dumpSendInstruction(stream, self, offset);
  case OP_POP_N:
  case OP_STORE_LOCAL:
  case OP_POPSTORE_LOCAL:
  case OP_LOAD_UPVALUE:
  case OP_INTERPOLATE:
  case OP_LOAD_LOCAL:
    return dumpByteInstruction(stream, self, offset);
  case OP_NPOPSTORE_STORE_LOCAL:
  case OP_NPOPSTORE_LOCAL:
  case OP_NLOAD_UPVALUE:
  case OP_NLOAD_LOCAL: {
    const char *name = gab_opcode_names[v_uint8_t_val_at(&self->bc, offset)];

    uint8_t operand = v_uint8_t_val_at(&self->bc, offset + 1);

    fprintf(stream, "%-25s%hhx: ", name, operand);

    for (int i = 0; i < operand - 1; i++) {
      fprintf(stream, "%hhx, ", v_uint8_t_val_at(&self->bc, offset + 2 + i));
    }

    fprintf(stream, "%hhx\n",
            v_uint8_t_val_at(&self->bc, offset + 1 + operand));

    return offset + 2 + operand;
  }
  case OP_RETURN:
    return dumpReturnInstruction(stream, self, offset);
  case OP_BLOCK: {
    offset++;

    uint16_t proto_constant =
        (((uint16_t)self->bc.data[offset] << 8) | self->bc.data[offset + 1]);

    offset += 2;

    gab_value pval = v_gab_value_val_at(&self->ks, proto_constant);

    struct gab_obj_prototype *p = GAB_VAL_TO_PROTOTYPE(pval);

    printf("%-25s" GAB_CYAN "%-20s\n" GAB_RESET, "OP_BLOCK",
           gab_strdata(&p->src->name));

    for (int j = 0; j < p->nupvalues; j++) {
      int isLocal = p->data[j] & fLOCAL_LOCAL;
      uint8_t index = p->data[j] >> 1;
      printf("      |                   %d %s\n", index,
             isLocal ? "local" : "upvalue");
    }
    return offset;
  }
  case OP_TRIM_UP1:
  case OP_TRIM_UP2:
  case OP_TRIM_UP3:
  case OP_TRIM_UP4:
  case OP_TRIM_UP5:
  case OP_TRIM_UP6:
  case OP_TRIM_UP7:
  case OP_TRIM_UP8:
  case OP_TRIM_UP9:
  case OP_TRIM_DOWN1:
  case OP_TRIM_DOWN2:
  case OP_TRIM_DOWN3:
  case OP_TRIM_DOWN4:
  case OP_TRIM_DOWN5:
  case OP_TRIM_DOWN6:
  case OP_TRIM_DOWN7:
  case OP_TRIM_DOWN8:
  case OP_TRIM_DOWN9:
  case OP_TRIM_EXACTLY0:
  case OP_TRIM_EXACTLY1:
  case OP_TRIM_EXACTLY2:
  case OP_TRIM_EXACTLY3:
  case OP_TRIM_EXACTLY4:
  case OP_TRIM_EXACTLY5:
  case OP_TRIM_EXACTLY6:
  case OP_TRIM_EXACTLY7:
  case OP_TRIM_EXACTLY8:
  case OP_TRIM_EXACTLY9:
  case OP_TRIM: {
    return dumpTrimInstruction(stream, self, offset);
  }
  default: {
    uint8_t code = v_uint8_t_val_at(&self->bc, offset);
    printf("Unknown opcode %d (%s?)\n", code, gab_opcode_names[code]);
    return offset + 1;
  }
  }
}

/*
 * Expand all the macros in the ast
 */
struct expand_res gab_expand(struct gab_triple gab, gab_value ast,
                             gab_value env) {
  assert(gab_valkind(ast) == kGAB_RECORD);
  assert(gab_valkind(env) == kGAB_RECORD);

  return expand_tuple(gab, ast, env);
}

gab_value gab_unquote(struct gab_triple gab, gab_value ast, gab_value env) {
  assert(gab_valkind(ast) == kGAB_RECORD);
  assert(gab_valkind(env) == kGAB_RECORD);

  struct bc bc = {0};

  struct expand_res res = gab_expand(gab, ast, env);

  unquote_tuple(gab, &bc, res.node, res.env);

  // nupvalues: need to track what upvalues are captured, all the way up callstack
  //   - environments need to track which values are captured, and capture them as well.
  // nslots:    need to track at compile time while unquoting, can just be on bc
  // nargs:     length of local env before expansion
  // nlocals:   length of local env after expansion
  //
  // call srcappend to append bytecode to src module
  // actually track bc_tok offset
  // addk should add constant to source
  // addk should check for common immediates
  //  - :ok, :err, :nil, :none, 1, 2

  uint64_t offset = 0;

  uint64_t end = bc.bc.len;

  while (offset < end) {
    fprintf(stdout, GAB_YELLOW "%04lu " GAB_RESET, offset);
    offset = dumpInstruction(stdout, &bc, offset);
  }

  return gab_undefined;
}
