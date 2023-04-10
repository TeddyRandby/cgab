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
  fprintf(stderr, "Error: %s", msg);
  print_help(stderr);
  exit(1);
}

boolean execute_arg(const char *flags) { return strchr(flags, 'e') != NULL; }

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

i32 main(i32 argc, const char **argv) {
  register_printf_specifier('V', gab_val_printf_handler,
                            gab_val_printf_arginfo);

  switch (argc) {
  case 1:
    gab_repl(GAB_FLAG_DUMP_ERROR);
    break;

  case 2:
    if (argv[1][0] == '-') {
      if (execute_arg(argv[1])) {
        throw_err("Not enough arguments for -e flag");
      }

      u8 flags = parse_flags(argv[1]);
      gab_repl(flags);
      return 0;
    }

    gab_run_file(argv[1], GAB_FLAG_DUMP_ERROR);
    break;

  case 3:
    if (argv[1][0] == '-') {
      u8 flags = parse_flags(argv[1]);
      if (execute_arg(argv[1])) {
        gab_run_string(argv[2], flags);
        return 0;
      }

      gab_run_file(argv[2], flags);
      return 0;
    }

    if (argv[2][0] == '-') {
      u8 flags = parse_flags(argv[2]);
      if (execute_arg(argv[2])) {
        gab_run_string(argv[1], flags);
        return 0;
      }

      gab_run_file(argv[1], flags);
      return 0;
    }

    throw_err("Too many arguments");
  default:
    print_help(stderr);
    return 1;
  }

  return 0;
}
