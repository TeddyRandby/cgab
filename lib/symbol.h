#include "include/gab.h"
#include <stdint.h>

gab_value symbol_create(gab_eg *gab, gab_vm *vm, gab_value name) {
  return gab_box(gab, vm,
                 (struct gab_box_argt){
                     .type = name,
                 });
}
