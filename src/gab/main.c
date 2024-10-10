#include "core.h"
#include "gab.h"
#include "os.h"

#include <curses.h>
#include <locale.h>
#include <printf.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#if GAB_OS_UNIX
#include <dlfcn.h>
#include <unistd.h>
#endif

#define TOSTRING(x) #x
#define STR(x) TOSTRING(x)

void *dynopen(const char *path) {
#if GAB_OS_UNIX
  void *dl = dlopen(path, RTLD_NOW);
  if (!dl) {
    printf("Failed opening '%s'\nERROR: %s\n", path, dlerror());
  }
  return dl;
#else
#error Windows not supported
#endif
}

void *dynsymbol(void *handle, const char *path) {
#if GAB_OS_UNIX
  return dlsym(handle, path);
#else
#error Windows not supported
#endif
}

#define MAIN_MODULE "gab\\main"

#define MAIN_WIN_COLOR 1

struct history_entry {
  char *source;
  const char *name;
  a_gab_value *result;
};

#define T struct history_entry
#define NAME history
#include "vector.h"

struct repl_model {
  struct gab_triple gab;

  /* INPUT WIDGET STATE */
  int iteration;

  int active_widget;
  bool should_exit;

  /* HISTORY WIDGET STATE */
  v_history history;
};

struct repl_model state;

enum repl_widget_flag {
  fREPL_WIDGET_FLEX_GROW = 1 << 0,
  fREPL_WIDGET_BORDER = 1 << 1,
};

struct repl_widget {
  WINDOW *w;
  int (*tick)(WINDOW *w, int key);
  int (*curs)(WINDOW *w, int key);
  int min_height;
  float height;
  int flags;
  enum repl_widget_kind {
    kREPL_WIDGET_FIBERS,
    kREPL_WIDGET_HISTORY,
    kREPL_WIDGET_INPUT,
  } k;
};

void run_src(struct gab_triple gab, s_char src, int flags, size_t jobs) {
  a_gab_value *result = gab_exec(gab, (struct gab_exec_argt){
                                          .name = MAIN_MODULE,
                                          .source = (char *)src.data,
                                          .flags = flags,
                                      });

  if (result) {
    gab_negkeep(gab.eg, result->len, result->data);

    a_gab_value_destroy(result);
  }
}

#define max(a, b)                                                              \
  ({                                                                           \
    typeof(a) _a = a;                                                          \
    typeof(b) _b = b;                                                          \
    _a > _b ? _a : _b;                                                         \
  })

// Keeps track of the terminal mode so we can reset the terminal if needed on
// errors
static bool visual_mode = false;

static noreturn void fail_exit(const char *msg) {
  // Make sure endwin() is only called in visual mode. As a note, calling it
  // twice does not seem to be supported and messed with the cursor position.
  if (visual_mode)
    endwin();
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

// Checks errors for (most) ncurses functions. CHECK(fn, x, y, z) is a checked
// version of fn(x, y, z).
#define CHECK(fn, ...)                                                         \
  do                                                                           \
    if (fn(__VA_ARGS__) == ERR)                                                \
      fail_exit(#fn "(" #__VA_ARGS__ ") failed");                              \
  while (false)

// Input character for readline
static unsigned char input;

// Used to signal "no more input" after feeding a character to readline
static bool input_avail = false;

// Calculates the cursor column for the readline window in a way that supports
// multibyte, multi-column and combining characters. readline itself calculates
// this as part of its default redisplay function and does not export the
// cursor column.
//
// Returns the total width (in columns) of the characters in the 'n'-byte
// prefix of the null-terminated multibyte string 's'. If 'n' is larger than
// 's', returns the total width of the string. Tries to emulate how readline
// prints some special characters.
//
// 'offset' is the current horizontal offset within the line. This is used to
// get tab stops right.
//
// Makes a guess for malformed strings.
static size_t strnwidth(const char *s, size_t n, size_t offset) {
  mbstate_t shift_state;
  wchar_t wc;
  size_t wc_len;
  size_t width = 0;

  // Start in the initial shift state
  memset(&shift_state, '\0', sizeof shift_state);

  for (size_t i = 0; i < n; i += wc_len) {
    // Extract the next multibyte character
    wc_len = mbrtowc(&wc, s + i, MB_CUR_MAX, &shift_state);
    switch (wc_len) {
    case 0:
      // Reached the end of the string
      goto done;

    case (size_t)-1:
    case (size_t)-2:
      // Failed to extract character. Guess that each character is one
      // byte/column wide each starting from the invalid character to
      // keep things simple.
      width += strnlen(s + i, n - i);
      goto done;
    }

    if (wc == '\t')
      width = ((width + offset + 8) & ~7) - offset;
    else
      // TODO: readline also outputs ~<letter> and the like for some
      // non-printable characters
      width += iswcntrl(wc) ? 2 : max(0, wcwidth(wc));
  }

done:
  return width;
}

// Like strnwidth, but calculates the width of the entire string
static size_t strwidth(const char *s, size_t offset) {
  return strnwidth(s, SIZE_MAX, offset);
}

// Not bothering with 'input_avail' and just returning 0 here seems to do the
// right thing too, but this might be safer across readline versions
static int readline_input_avail(void) { return input_avail; }

static int readline_getc(FILE *dummy) {
  input_avail = false;
  return input;
}

/*static int readline_redisplay();*/

static void forward_to_readline(char c) {
  input = c;
  input_avail = true;
  rl_callback_read_char();
}

static void got_command(char *line) {
  if (!line) {
    state.should_exit = true;
    return;
  }

  struct history_entry *h = v_history_emplace(&state.history);
  size_t len = strnwidth(rl_line_buffer, rl_point, 0);
  h->source = malloc(len + 1);
  strncpy(h->source, rl_line_buffer, len + 1);

  size_t unique_name_len = strlen(MAIN_MODULE) + 16;
  char *unique_name = malloc(unique_name_len);
  snprintf(unique_name, unique_name_len, "%s:%03i", MAIN_MODULE,
           state.iteration++);

  h->result = gab_exec(state.gab, (struct gab_exec_argt){
                                      .name = unique_name,
                                      .source = (char *)h->source,
                                  });

  h->name = unique_name;
}

static void init_ncurses(void) {
  if (!initscr())
    fail_exit("Failed to initialize ncurses");
  visual_mode = true;

  if (has_colors()) {
    CHECK(start_color);
    CHECK(use_default_colors);
  }

  CHECK(cbreak);
  CHECK(noecho);
  CHECK(nonl);
  CHECK(intrflush, NULL, FALSE);
  timeout(64);
  // Do not enable keypad() since we want to pass unadulterated input to
  // readline

  // Explicitly specify a "very visible" cursor to make sure it's at least
  // consistent when we turn the cursor on and off (maybe it would make sense
  // to query it and use the value we get back too). "normal" vs. "very
  // visible" makes no difference in gnome-terminal or xterm. Let this fail
  // for terminals that do not support cursor visibility adjustments.
  curs_set(2);
}

static void deinit_ncurses(void) {
  CHECK(endwin);
  visual_mode = false;
}

static void init_readline(void) {
  // Disable completion. TODO: Is there a more robust way to do this?
  if (rl_bind_key('\t', rl_insert))
    fail_exit("Invalid key passed to rl_bind_key()");

  // Let ncurses do all terminal and signal handling
  rl_catch_signals = 0;
  rl_catch_sigwinch = 0;
  rl_deprep_term_function = NULL;
  rl_prep_term_function = NULL;

  // Prevent readline from setting the LINES and COLUMNS environment
  // variables, which override dynamic size adjustments in ncurses. When
  // using the alternate readline interface (as we do here), LINES and
  // COLUMNS are not updated if the terminal is resized between two calls to
  // rl_callback_read_char() (which is almost always the case).
  rl_change_environment = 0;

  // Handle input by manually feeding characters to readline
  rl_getc_function = readline_getc;
  rl_input_available_hook = readline_input_avail;
  /*rl_redisplay_function = readline_redisplay;*/

  rl_callback_handler_install("> ", got_command);
}

static void deinit_readline(void) { rl_callback_handler_remove(); }

int tick_history(WINDOW *w, int key) {
  int maxy, maxx;
  getmaxyx(w, maxy, maxx);

  size_t len = state.history.len;
  for (size_t i = 0; i < len; i++) {
    int line = i * 2 + 1;
    struct history_entry *h = v_history_ref_at(&state.history, len - i - 1);

    if (line + 2 >= maxy)
      break;

    mvwprintw(w, line, 1, "%s > %s", h->name, (char *)h->source);

    if (!h->result)
      continue;

    mvwprintw(w, i * 2 + 2, 1, " => ");
    size_t vlen = h->result->len;
    for (size_t j = 0; j < vlen; j++) {
      gab_value vs = gab_valintos(state.gab, h->result->data[j]);
      const char *cs = gab_strdata(&vs);

      waddstr(w, cs);
      if (j + 1 != vlen)
        waddstr(w, ", ");
    }
  }

  return OK;
}

int curs_input(WINDOW *w, int key) {
  if (key != ERR && key < UINT8_MAX)
    forward_to_readline(key);

  size_t prompt_width = strwidth(rl_display_prompt, 0);

  size_t cursor_col =
      prompt_width + strnwidth(rl_line_buffer, rl_point, prompt_width) + 1;

  wmove(w, 1, cursor_col);

  wrefresh(w);
}

int tick_fibers(WINDOW *w, int key) {
  const char *msg = "gab\\repl";
  mvwprintw(w, 0, COLS - 2 - strlen(msg), "%s", msg);

  wmove(w, 1, 1);

  int len = state.gab.eg->len;
  for (int i = 0; i < len; i++) {
    wprintw(w, "%i [%s] ", i, state.gab.eg->jobs[i].alive ? "•" : " ");
  }
}

int tick_input(WINDOW *w, int key) {
  // Allow strings longer than the message window and show only the last part
  // if the string doesn't fit
  CHECK(scrollok, w, TRUE);

  mvwprintw(w, 1, 1, "%s%s", rl_display_prompt, rl_line_buffer);

  return OK;
};

struct repl_widget layout[] = {
    {
        .tick = tick_fibers,
        .curs = nullptr,
        .min_height = 3,
        .height = 0.1,
        .flags = fREPL_WIDGET_BORDER,
        .k = kREPL_WIDGET_FIBERS,
    },
    {
        .tick = tick_input,
        .curs = curs_input,
        .min_height = 3,
        .height = 0.1,
        .k = kREPL_WIDGET_INPUT,
    },
    {
        .tick = tick_history,
        .curs = nullptr,
        .min_height = 4,
        .flags = fREPL_WIDGET_FLEX_GROW | fREPL_WIDGET_BORDER,
        kREPL_WIDGET_HISTORY,
    },
};

#define NWIDGETS (sizeof(layout) / sizeof(struct repl_widget))

int getpos(struct repl_widget *wid);

int getheight(struct repl_widget *wid) {
  if (wid->flags & fREPL_WIDGET_FLEX_GROW)
    return LINES - getpos(wid);

  int flex = wid->height * LINES;
  return flex < wid->min_height ? wid->min_height : flex;
}

int getpos(struct repl_widget *wid) {
  size_t offset = wid - layout;
  int above = 0;

  for (size_t i = 0; i < offset; i++)
    above += getheight(layout + i);

  return above;
}

int tick_widget(struct repl_widget *wid, int key) {
  int nlines = getheight(wid), npos = getpos(wid);

  if (wid->w == nullptr)
    wid->w = newwin(nlines, 0, npos, 0);

  if (!wid->w)
    return ERR;

  if (wresize(wid->w, nlines, COLS) != OK)
    return delwin(wid->w), wid->w = nullptr, ERR;

  if (mvwin(wid->w, npos, 0) != OK)
    return delwin(wid->w), wid->w = nullptr, ERR;

  wclear(wid->w);

  if (wid->flags & fREPL_WIDGET_BORDER)
    box(wid->w, 0, 0);

  if (wid->tick)
    wid->tick(wid->w, key);

  wnoutrefresh(wid->w);

  return OK;
}

void run_repl(int flags) {
  state.active_widget = 1; // The input is active initially

  state.gab = gab_create((struct gab_create_argt){
      .os_dynopen = dynopen,
      .os_dynsymbol = dynsymbol,
      .flags = flags | fGAB_ERR_QUIET,
  });

  if (!setlocale(LC_ALL, ""))
    fail_exit("Failed to set locale attributes from environment");

  init_ncurses();
  init_readline();

  for (;;) {
    if (state.should_exit)
      break;

    int key = wgetch(stdscr);

    for (int wid = 0; wid < NWIDGETS; wid++)
      tick_widget(layout + wid, key);

    if (layout[state.active_widget].w && layout[state.active_widget].curs)
      layout[state.active_widget].curs(layout[state.active_widget].w, key);

    doupdate();
  }

  deinit_ncurses();
  deinit_readline();

  gab_destroy(state.gab);
  return;
}

void run_string(const char *string, int flags, size_t jobs) {
  struct gab_triple gab = gab_create((struct gab_create_argt){
      .os_dynopen = dynopen,
      .os_dynsymbol = dynsymbol,
      .flags = flags,
      .jobs = jobs,
  });

  // This is a weird case where we actually want to include the null terminator
  s_char src = s_char_create(string, strlen(string) + 1);

  run_src(gab, src, flags, 8);

  gab_destroy(gab);
  return;
}

void run_file(const char *path, int flags, size_t jobs) {
  struct gab_triple gab = gab_create((struct gab_create_argt){
      .os_dynopen = dynopen,
      .os_dynsymbol = dynsymbol,
      .flags = flags,
      .jobs = jobs,
  });

  a_gab_value *result = gab_suse(gab, path);

  if (!result)
    fprintf(stdout, "[gab]: Module '%s' not found.\n", path);

  gab_destroy(gab);
  return;
}

struct option {
  const char *name;
  const char *desc;
  char shorthand;
  bool takes_argument;
  int flag;
};

#define MAX_OPTIONS 7

struct command {
  const char *name;
  const char *desc;
  int (*handler)(int, const char **, int);
  struct option options[MAX_OPTIONS];
};

int run(int argc, const char **argv, int flags);
int exec(int argc, const char **argv, int flags);
int repl(int argc, const char **argv, int flags);
int help(int argc, const char **argv, int flags);

#define DEFAULT_COMMAND commands[0]

static struct command commands[] = {
    {
        "help",
        "Print this help message",
        .handler = help,
    },
    {
        "run",
        "Compile and run the module at path <args>",
        .handler = run,
        {
            {
                "dump",
                "Dump compiled ast to stdout",
                'a',
                .flag = fGAB_AST_DUMP,
            },
            {
                "dump",
                "Dump compiled bytecode to stdout",
                'd',
                .flag = fGAB_BUILD_DUMP,
            },
            {
                "quiet",
                "Do not print errors to stderr",
                'q',
                .flag = fGAB_ERR_QUIET,
            },
            {
                "sterr",
                "Instead of pretty-printing errors, use a structured output.",
                's',
                .flag = fGAB_ERR_STRUCTURED,
            },
            {
                "check",
                "Compile the file without running it.",
                'c',
                .flag = fGAB_BUILD_CHECK,
            },
            {
                "eenv",
                "Don't use gab's core module - start with a mostly-empty "
                "environment.",
                'e',
                .flag = fGAB_ENV_EMPTY,
            },
            {
                "jobs",
                "Specify the number of os threads which should serve as "
                "workers for running gab fibers. Default is " STR(
                    cGAB_DEFAULT_NJOBS) ".",
                'j',
                .flag = fGAB_JOB_RUNNERS,
            },
        },
    },
    {
        "exec",
        "Compile and run the string <args>",
        .handler = exec,
        {
            {
                "dump",
                "Dump compiled ast to stdout",
                'a',
                .flag = fGAB_AST_DUMP,
            },
            {
                "dump",
                "Dump compiled bytecode to stdout",
                'd',
                .flag = fGAB_BUILD_DUMP,
            },
            {
                "quiet",
                "Do not print errors to stderr",
                'q',
                .flag = fGAB_ERR_QUIET,
            },
            {
                "sterr",
                "Instead of pretty-printing errors, use a structured output.",
                's',
                .flag = fGAB_ERR_STRUCTURED,
            },
            {
                "check",
                "Compile the file without running it.",
                'c',
                .flag = fGAB_BUILD_CHECK,
            },
            {
                "eenv",
                "Don't use gab's core module - start with a mostly-empty "
                "environment.",
                'e',
                .flag = fGAB_ENV_EMPTY,
            },
        },
    },
    {
        "repl",
        "Enter the read-eval-print loop.",
        .handler = repl,
        {
            {
                "dump",
                "Dump compiled bytecode to stdout",
                'd',
                .flag = fGAB_BUILD_DUMP,
            },
            {
                "eenv",
                "Don't use gab's core module - start with a mostly-empty "
                "environment.",
                'e',
                .flag = fGAB_ENV_EMPTY,
            },
        },
    },
};

#define N_COMMANDS (LEN_CARRAY(commands))

struct parse_options_result {
  int remaining;
  int flags;
};

struct parse_options_result parse_options(int argc, const char **argv,
                                          struct command command) {
  int flags = 0;

  for (int i = 0; i < argc; i++) {
    if (argv[i][0] != '-')
      return (struct parse_options_result){argc - i, flags};

    if (argv[i][1] == '-') {
      for (int j = 0; j < MAX_OPTIONS; j++) {
        struct option opt = command.options[j];

        if (opt.name && !strcmp(argv[i] + 2, opt.name)) {
          flags |= opt.flag;
          break;
        }
      }

      printf("UNRECOGNIZED FLAG: %s\n", argv[i]);
      exit(1);
      continue;
    } else {
      for (int j = 0; j < MAX_OPTIONS; j++) {
        struct option opt = command.options[j];

        if (opt.name && argv[i][1] == opt.shorthand) {
          flags |= opt.flag;
          break;
        }
      }

      continue;

      printf("UNRECOGNIZED FLAG: %s\n", argv[i]);
      exit(1);
    }
  }

  return (struct parse_options_result){0, flags};
}

int run(int argc, const char **argv, int flags) {
  assert(argc > 0);

  const char *path = argv[0];
  size_t jobs = 8;

  if (flags & fGAB_JOB_RUNNERS) {
    const char *njobs = argv[0];

    if (argc < 2) {
      printf("ERROR: Not enough arguments\n");
      return 1;
    }

    jobs = atoi(njobs);
    path = argv[1];
  }

  run_file(path, flags, jobs);

  return 0;
}

int exec(int argc, const char **argv, int flags) {
  assert(argc > 0);

  run_string(argv[0], flags, 8);

  return 0;
}

int repl(int argc, const char **argv, int flags) {
  run_repl(flags);
  return 0;
}

int help(int argc, const char **argv, int flags) {
  for (int i = 0; i < N_COMMANDS; i++) {
    struct command cmd = commands[i];
    printf("\ngab %4s [opts] <args>\t%s\n", cmd.name, cmd.desc);

    for (int j = 0; j < MAX_OPTIONS; j++) {
      struct option opt = cmd.options[j];

      if (!opt.name)
        break;

      printf("\t--%-5s\t-%c\t%s\n", opt.name, opt.shorthand, opt.desc);
    }
  };
  return 0;
}

int main(int argc, const char **argv) {
  register_printf_specifier('V', gab_val_printf_handler,
                            gab_val_printf_arginfo);

  if (argc < 2)
    goto fin;

  for (int i = 0; i < N_COMMANDS; i++) {
    struct command cmd = commands[i];
    assert(cmd.handler);

    if (!strcmp(argv[1], cmd.name)) {
      struct parse_options_result o = parse_options(argc - 2, argv + 2, cmd);

      return cmd.handler(o.remaining, argv + (argc - o.remaining), o.flags);
    }
  }

fin: {
  struct command cmd = DEFAULT_COMMAND;
  return cmd.handler(0, argv, 0);
}
}
