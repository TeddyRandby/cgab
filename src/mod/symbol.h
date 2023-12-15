#include "gab.h"

gab_value symbol_create(struct gab_triple gab, gab_value name) {
  return gab_box(gab, (struct gab_box_argt){
                          .type = name,
                      });
}
