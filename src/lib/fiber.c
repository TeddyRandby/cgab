#include "include/alloc.h"
#include "include/gab.h"
#include "include/types.h"

#include <stdint.h>
#include <threads.h>

i32 gab_thread_run(void *data) {
  gab_value block = (uintptr_t)data;

  gab_val_dump(stderr, block);

  gab_engine *gab = gab_create(alloc_setup());

  GAB_SEND(GAB_MESSAGE_CAL, block, 0, NULL, NULL);

  alloc_teardown(gab_user(gab));

  return 42;
}

gab_value gab_lib_new(gab_engine *gab, gab_vm *vm, u8 argc,
                      gab_value argv[argc]) {

  if (!GAB_VAL_IS_BLOCK(argv[1]))
    return gab_panic(gab, vm, "Invalid call to gab_lib_new");

  thrd_t split;

  if (thrd_create(&split, gab_thread_run, (void *)(uintptr_t)argv[1]) !=
      thrd_success)
    return gab_panic(gab, vm, "Failed to create thread");

  thrd_join(split, NULL);

  return GAB_VAL_NIL();
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("new"),
  };

  gab_value type_fiber = GAB_SYMBOL("Fiber");

  gab_dref(gab, vm, type_fiber);

  gab_value receivers[] = {
      type_fiber,
  };

  gab_value values[] = {
      GAB_BUILTIN(new),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(values));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));

  for (int i = 0; i < LEN_CARRAY(names); i++) {
    gab_specialize(gab, names[i], receivers[i], values[i]);
    gab_dref(gab, vm, values[i]);
  }

  return type_fiber;
}
