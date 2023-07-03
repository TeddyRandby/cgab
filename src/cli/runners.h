#include "include/gab.h"

void repl(const char *module, u8 flags);

void run_file(const char *path, const char *module, char delim, u8 flags);

void run_string(const char *string, const char *module, char delim, u8 flags);
