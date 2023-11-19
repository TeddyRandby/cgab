#include "core.h"
#include <stdio.h>

a_char *gab_osread(const char *path);

a_char *gab_fosread(FILE* fd);

a_char *gab_fosreadl(FILE* fd);

a_char *gab_oscwd();

void *gab_osdlopen(const char* path);

void *gab_osdlsym(void* handle, const char* path);

void gab_osdlclose(void* handle);
