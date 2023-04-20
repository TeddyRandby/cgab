#include "include/gab.h"
#include <stdint.h>

gab_obj_container* symbol_create(gab_engine* gab, gab_vm* vm, gab_value name) {
    return gab_obj_container_create(gab, vm, name, NULL, NULL, (void*)name);
}
