#include "core.h"
#include "gab.h"
#include "os.h"
#include <printf.h>
#include <stdio.h>
#include <string.h>

#if GAB_OS_UNIX
#include <dlfcn.h>
#include <unistd.h>
#endif

void *dynopen(const char *path) {
#if GAB_OS_UNIX
  return dlopen(path, RTLD_NOW);
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

#define MAIN_MODULE "__main__"

void run_repl(int flags) {
  struct gab_triple gab = gab_create((struct gab_create_argt){
      .os_dynopen = dynopen,
      .os_dynsymbol = dynsymbol,
  });

  gab_suse(gab, "core");

  gab_repl(gab, (struct gab_repl_argt){
                    .name = "repl",
                    .prompt_prefix =
                        "gab:" GAB_VERSION_MAJOR "." GAB_VERSION_MINOR "> ",
                    .result_prefix = "=> ",
                    .flags = flags,
                });

  gab_destroy(gab);
  return;
}

void run_src(struct gab_triple gab, s_char src, int flags) {
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

void run_string(const char *string, int flags) {
  struct gab_triple gab = gab_create((struct gab_create_argt){
      .os_dynopen = dynopen,
      .os_dynsymbol = dynsymbol,
  });

  // This is a weird case where we actually want to include the null terminator
  s_char src = s_char_create(string, strlen(string) + 1);

  run_src(gab, src, flags);

  gab_destroy(gab);
  return;
}

void run_file(const char *path, int flags) {
  struct gab_triple gab = gab_create((struct gab_create_argt){
      .os_dynopen = dynopen,
      .os_dynsymbol = dynsymbol,
      .flags = flags,
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

#define MAX_OPTIONS 4

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
        "Compile and run the module at path <arg>",
        .handler = run,
        {
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
        },
    },
    {
        "exec",
        "Compile and run the string <arg>",
        .handler = exec,
        {
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
        },
    },
    {
        "repl",
        "Enter the read-eval-print loop.",
        .handler = repl,
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

      continue;
    }

    for (int j = 0; j < MAX_OPTIONS; j++) {
      struct option opt = command.options[j];

      if (opt.name && argv[i][1] == opt.shorthand) {
        flags |= opt.flag;
        break;
      }
    }
  }

  return (struct parse_options_result){0, flags};
}

int run(int argc, const char **argv, int flags) {
  assert(argc > 0);

  run_file(argv[0], flags);

  return 0;
}

int exec(int argc, const char **argv, int flags) {
  assert(argc > 0);

  run_string(argv[0], flags);

  return 0;
}

int repl(int argc, const char **argv, int flags) {
  run_repl(flags);
  return 0;
}

int help(int argc, const char **argv, int flags) {
  for (int i = 0; i < N_COMMANDS; i++) {
    struct command cmd = commands[i];
    printf("\ngab %4s [opts] <arg>\t%s\n", cmd.name, cmd.desc);

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

fin:
  struct command cmd = DEFAULT_COMMAND;
  return cmd.handler(0, argv, 0);
}
