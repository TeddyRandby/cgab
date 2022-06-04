#include "memory.h"
#include "../gab/vm/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void copy(void *dest, const void *src, u64 size) { memcpy(dest, src, size); }
