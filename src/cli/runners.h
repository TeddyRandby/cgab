#include "include/gab.h"

void repl(const char *module, uint8_t flags);

void run_file(const char *path, const char *module, char delim, uint8_t flags);

void run_string(const char *string, const char *module, char delim, uint8_t flags);
