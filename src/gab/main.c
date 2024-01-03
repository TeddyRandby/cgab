#include "core.h"
#include "gab.h"
#include "os.h"
#include "runners.h"
#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help(FILE *stream) {
  fprintf(stream, "Usage: gab [options] <path>\n"
                  "\t-q\tQuiet mode\n"
                  "\t-b\tDump bytecode to stdout\n"
                  "\t-e\tExecute argument as code\n"
                  "\t-r\tRequire argument as module\n"
                  "\t-s\tStream input from stdin\n"
                  "\t-d\tDelimit input from stdin by newline\n"
                  "\t-h\tPrint this help message\n"
                  "\t-v\tPrint version\n");
}

void throw_err(const char *msg) {
  fprintf(stderr, "Error: %s\n", msg);
  print_help(stderr);
  exit(1);
}

bool execute_arg(const char *flags) { return strchr(flags, 'e') != NULL; }

bool module_arg(const char *flags) { return strchr(flags, 'r') != NULL; }

bool delimit_arg(const char *flags) { return strchr(flags, 'd') != NULL; }

uint8_t parse_flags(const char *arg) {
  uint8_t flags = fGAB_DUMP_ERROR;

  uint8_t i = 0;
  for (;;) {
    const char c = arg[++i];

    switch (c) {
    case 'q':
      flags &= ~fGAB_DUMP_ERROR;
      break;
    case 'b':
      flags |= fGAB_DUMP_BYTECODE;
      break;
    case 's':
      flags |= fGAB_STREAM_INPUT;
      break;
    case 'd':
      flags |= fGAB_DELIMIT_INPUT;
      break;
    case '\0':
      return flags;
    case 'r':
    case 'e': // Special case- ignore
      break;
    case 'h':
      print_help(stdout);
      exit(0);
    case 'v':
      printf("gab " GAB_VERSION_MAJOR "." GAB_VERSION_MINOR "\n");
      exit(0);
    default:
      throw_err("Unknown flag");
    }
  }
}

int32_t run_3_args(const char *arg_flags, const char *arg1) {
  uint8_t flags = parse_flags(arg_flags);

  if (execute_arg(arg_flags)) {
    if (module_arg(arg_flags))
      throw_err("Too few arguments for e and r flags");

    if (module_arg(arg_flags))
      throw_err("Too few arguments for e and d flags");

    run_string(arg1, NULL, 0, flags);
    return 0;
  }

  if (module_arg(arg_flags)) {
    repl(arg1, flags);
    return 0;
  }

  run_file(arg1, NULL, 0, flags);
  return 0;
}

int32_t run_4_args(const char *arg_flags, const char *arg1, const char *arg2) {
  uint8_t flags = parse_flags(arg_flags);

  if (execute_arg(arg_flags)) {
    if (module_arg(arg_flags)) {

      if (delimit_arg(arg_flags))
        throw_err("Too few arguments for e, r, and d flags");

      run_string(arg2, arg1, 0, flags);
      return 0;
    }

    if (delimit_arg(arg_flags)) {
      if (module_arg(arg_flags))
        throw_err("Too few arguments for e, r, and d flags");

      run_string(arg2, NULL, arg1[0], flags);
      return 0;
    }

    throw_err("Too many arguments");
  }

  if (module_arg(arg_flags)) {
    run_file(arg2, arg1, 0, flags);
    return 0;
  }

  throw_err("Too many arguments");
  return 1;
}

int32_t main(int32_t argc, const char **argv) {
  register_printf_specifier('V', gab_val_printf_handler,
                            gab_val_printf_arginfo);

  switch (argc) {
  case 1:
    repl(NULL, fGAB_DUMP_ERROR);
    return 0;

  case 2:
    if (argv[1][0] == '-') {
      if (execute_arg(argv[1]))
        throw_err("Not enough arguments for -e flag");

      if (module_arg(argv[1]))
        throw_err("Not enough arguments for -r flag");

      if (delimit_arg(argv[1]))
        throw_err("Not enough arguments for -d flag");

      uint8_t flags = parse_flags(argv[1]);
      repl(NULL, flags);
      return 0;
    }

    run_file(argv[1], NULL, 0, fGAB_DUMP_ERROR | fGAB_EXIT_ON_PANIC);
    return 0;

  case 3:
    if (argv[1][0] == '-')
      return run_3_args(argv[1], argv[2]);

    throw_err("Too many arguments");

  case 4:
    if (argv[1][0] == '-')
      return run_4_args(argv[1], argv[2], argv[3]);

    throw_err("Too many arguments");

  default:
    print_help(stderr);
    return 1;
  }

  return 0;
}
