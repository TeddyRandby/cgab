#include "include/alloc.h"
#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/os.h"
#include "include/value.h"
#include "runners.h"
#include <assert.h>
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

[[noreturn]] void throw_err(const char *msg) {
  fprintf(stderr, "Error: %s\n", msg);
  print_help(stderr);
  exit(1);
}

boolean execute_arg(const char *flags) { return strchr(flags, 'e') != NULL; }

boolean module_arg(const char *flags) { return strchr(flags, 'r') != NULL; }

u8 parse_flags(const char *arg) {
  u8 flags = GAB_FLAG_DUMP_ERROR;

  u8 i = 0;
  for (;;) {
    const char c = arg[++i];

    switch (c) {
    case 'q':
      flags &= ~GAB_FLAG_DUMP_ERROR;
      break;
    case 'b':
      flags |= GAB_FLAG_DUMP_BYTECODE;
      break;
    case 's':
      flags |= GAB_FLAG_STREAM_INPUT;
      break;
    case 'd':
      flags |= GAB_FLAG_DELIMIT_INPUT;
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
      printf("gab %d.%d\n", GAB_VERSION_MAJOR, GAB_VERSION_MINOR);
      exit(0);
    default:
      throw_err("Unknown flag");
    }
  }
}

i32 run_3_args(const char *arg_flags, const char *arg1) {
  u8 flags = parse_flags(arg_flags);

  if (execute_arg(arg_flags)) {
    if (module_arg(arg_flags))
      throw_err("Too few arguments for -e and -r flags");

    gab_run_string(arg1, NULL, flags);
    return 0;
  }

  if (module_arg(arg_flags)) {
    gab_repl(arg1, flags);
    return 0;
  }

  gab_run_file(arg1, NULL, flags);
  return 0;
}

i32 run_4_args(const char *arg_flags, const char *arg1, const char *arg2) {
  u8 flags = parse_flags(arg_flags);

  if (execute_arg(arg_flags)) {
    if (module_arg(arg_flags)) {
      gab_run_string(arg2, arg1, flags);
      return 0;
    }

    throw_err("Too many arguments");
  }

  if (module_arg(arg_flags)) {
    gab_run_file(arg2, arg1, flags);
    return 0;
  }

  throw_err("Too many arguments");
}

i32 main(i32 argc, const char **argv) {
  register_printf_specifier('V', gab_val_printf_handler,
                            gab_val_printf_arginfo);

  switch (argc) {
  case 1:
    gab_repl(NULL, GAB_FLAG_DUMP_ERROR);
    break;

  case 2:
    if (argv[1][0] == '-') {
      if (execute_arg(argv[1])) {
        throw_err("Not enough arguments for -e flag");
      }

      if (module_arg(argv[1])) {
        throw_err("Not enough arguments for -r flag");
      }

      u8 flags = parse_flags(argv[1]);
      gab_repl(NULL, flags);
      return 0;
    }

    gab_run_file(argv[1], NULL, GAB_FLAG_DUMP_ERROR);
    break;

  case 3:
    if (argv[1][0] == '-')
      return run_3_args(argv[1], argv[2]);

    if (argv[2][0] == '-')
      return run_3_args(argv[2], argv[1]);

    throw_err("Too many arguments");

  case 4:
    if (argv[1][0] == '-')
      return run_4_args(argv[1], argv[2], argv[3]);

    if (argv[2][0] == '-')
      return run_4_args(argv[2], argv[1], argv[3]);

    if (argv[3][0] == '-')
      return run_4_args(argv[3], argv[1], argv[2]);

    throw_err("Too many arguments");

  default:
    print_help(stderr);
    return 1;
  }

  return 0;
}
