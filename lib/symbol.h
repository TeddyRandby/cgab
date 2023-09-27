#include "include/gab.h"
#include <stdint.h>

gab_value symbol_create(struct gab_eg *gab, struct gab_gc *gc,
                        struct gab_vm *vm, gab_value name) {
  return gab_box(gab, (struct gab_box_argt){
                          .type = gab_gciref(gab, gc, vm, name),
                      });
}
