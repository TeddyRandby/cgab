#include "gab.h"
#include <stdio.h>

a_gab_value *gab_osloaddynmod(struct gab_triple gab, const char *path);

a_char *gab_osread(const char *path);

a_char *gab_fosread(FILE *fd);

a_char *gab_fosreadl(FILE *fd);
