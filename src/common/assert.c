#include "types.h"
#include <stdio.h>
#include <stdlib.h>

void failure(const char *file, const u32 line, const char *msg) {
  fprintf(stderr, "Assertion failed in %s at line %u\nReason: %s\n", file, line,
          msg);
  exit(1);
}
