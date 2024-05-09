#include "core.h"
#include "gab.h"
#include "os.h"
#include "runners.h"
#include <printf.h>
#include <stdio.h>
#include <string.h>

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
int send(int argc, const char **argv, int flags);
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
        "Compile and run the file at path <arg>",
        .handler = run,
        {
            {
                "dump",
                "Dump compiled bytecode to stdout",
                'd',
                fGAB_DUMP_BYTECODE,
            },
            {
                "quiet",
                "Do not print errors to stderr",
                'q',
                fGAB_QUIET,
            },
            // {
            //   "use",
            //   "Import the given module with \\use before running the file",
            //   'u',
            //   fGAB_USE,
            // },
        },
    },
    {
        "exec",
        "Compile and run the string <arg>",
        .handler = exec,
    },
    {
      "send",
      "Send the message <arg 1> to the value <arg 2>",
      .handler = send,
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
      for (int i = 0; i < MAX_OPTIONS; i++) {
        struct option opt = command.options[i];

        if (opt.name && !strcmp(argv[i] + 2, opt.name)) {
          flags |= opt.flag;
        }
      }
    } else {
      for (int i = 0; i < MAX_OPTIONS; i++) {
        struct option opt = command.options[i];

        if (opt.name && argv[i][1] == opt.shorthand) {
          flags |= opt.flag;
        }
      }
    }
  }

  return (struct parse_options_result){0, flags};
}

int run(int argc, const char **argv, int flags) {
  assert(argc > 0);

  run_file(argv[0], nullptr, 0, flags);

  return 0;
}

int exec(int argc, const char **argv, int flags) {
  assert(argc > 0);

  run_string(argv[0], nullptr, 0, flags);

  return 0;
}

int repl(int argc, const char **argv, int flags) {
  run_repl(nullptr, flags);
  return 0;
}

int send(int argc, const char **argv, int flags) {
  assert(argc > 1);
  run_send(argv[0], argv[1]);
  return 0;
}

int help(int argc, const char **argv, int flags) {
  for (int i = 0; i < N_COMMANDS; i++) {
    struct command cmd = commands[i];
    printf("gab %4s [opts] <args>\t%s\n", cmd.name, cmd.desc);

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
