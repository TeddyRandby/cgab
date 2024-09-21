#include "colors.h"
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

static gab_token prev_tok(struct parser *parser) {
  return v_gab_token_val_at(&parser->src->tokens, parser->offset - 1);
}

static s_char prev_src(struct parser *parser) {
  return v_s_char_val_at(&parser->src->token_srcs, parser->offset - 1);
}

gab_value parse_exp_num(struct gab_triple gab, struct parser *parser,
                        gab_value lhs) {
  double num = strtod((char *)prev_src(parser).data, nullptr);
  return gab_number(num);
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

gab_value parse_expression(struct gab_triple gab, struct parser *parser,
                           enum prec_k prec);

static inline void skip_newlines(struct gab_triple gab, struct parser *parser) {
  while (match_and_eat_token(gab, parser, TOKEN_NEWLINE))
    ;
}

bool is_multi(gab_value node) {
  return gab_valkind(node) == kGAB_RECORD; // Check that the node is a send node
}

gab_value wrap_in_trim(struct gab_triple gab, gab_value node, uint8_t want) {
  // Wrap the given node in a send to \gab.runtime.trim
  return node;
}

static gab_value parse_expressions_body(struct gab_triple gab,
                                          struct parser *parser) {
  gab_value result = gab_undefined;

  skip_newlines(gab, parser);

  result = parse_expression(gab, parser, kASSIGNMENT);

  if (result == gab_undefined)
    return gab_undefined;

  skip_newlines(gab, parser);

  while (!match_terminator(parser) && !match_token(parser, TOKEN_EOF)) {
    if (is_multi(result))
      result = wrap_in_trim(gab, result, 0);

    result = parse_expression(gab, parser, kASSIGNMENT);

    if (result == gab_undefined)
      return gab_undefined;

    skip_newlines(gab, parser);
  }

  return result;
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

const struct parse_rule parse_rules[] = {
    {nullptr, nullptr, kNONE},       // DO
    {nullptr, nullptr, kNONE},       // END
    {nullptr, nullptr, kNONE},       // DOT
    {nullptr, nullptr, kNONE},       // COMMA
    {nullptr, nullptr, kNONE},       // EQUAL
    {nullptr, nullptr, kNONE},       // COLON
    {nullptr, nullptr, kNONE},       // LBRACE
    {nullptr, nullptr, kNONE},       // RBRACE
    {nullptr, nullptr, kNONE},       // LBRACK
    {nullptr, nullptr, kNONE},       // RBRACK
    {nullptr, nullptr, kNONE},       // LPAREN
    {nullptr, nullptr, kNONE},       // RPAREN
    {nullptr, nullptr, kNONE},       // SEND
    {nullptr, nullptr, kNONE},       // OPERATOR
    {nullptr, nullptr, kNONE},       // IDENTIFIER
    {nullptr, nullptr, kNONE},       // SIGIL
    {nullptr, nullptr, kNONE},       // STRING
    {nullptr, nullptr, kNONE},       // INTERPOLATION END
    {nullptr, nullptr, kNONE},       // INTERPOLATION MIDDLE
    {nullptr, nullptr, kNONE},       // INTERPOLATION END
    {parse_exp_num, nullptr, kNONE}, // NUMBER
    {nullptr, nullptr, kNONE},       // MESSAGE
    {nullptr, nullptr, kNONE},       // NEWLINE
    {nullptr, nullptr, kNONE},       // EOF
    {nullptr, nullptr, kNONE},       // ERROR
};
struct parse_rule get_parse_rule(gab_token k) { return parse_rules[k]; }

gab_value parse(struct gab_triple gab, struct parser *parser,
                uint8_t narguments, gab_value arguments[narguments]) {
  /*push_ctxframe(parser);*/
  /*push_trim(parser, narguments, parser->offset - 1);*/

  /*int ctx = peek_ctx(parser, kFRAME, 0);*/
  /*struct frame *f = &parser->contexts[ctx].as.frame;*/

  /*f->narguments = narguments;*/

  if (curr_tok(parser) == TOKEN_EOF)
    return gab_undefined;

  if (curr_tok(parser) == TOKEN_ERROR) {
    eat_token(gab, parser);
    parser_error(gab, parser, GAB_MALFORMED_TOKEN,
                 "This token is malformed or unrecognized.");
    return gab_undefined;
  }

  /*for (uint8_t i = 0; i < narguments; i++)*/
  /*  init_local(parser, add_local(parser, arguments[i], 0));*/

  gab_value result = parse_expressions_body(gab, parser);

  if (result == gab_undefined)
    return gab_undefined;

  if (gab.flags & fGAB_AST_DUMP)
    gab_fvalinspect(stdout, result, -1);

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

  gab_value module = parse(gab, &parser, args.len, vargv);

  gab_gcunlock(gab);

  return module;
}
