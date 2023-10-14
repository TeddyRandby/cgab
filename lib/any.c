#include "include/core.h"
#include "include/gab.h"

void gab_lib_send(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm,
                  size_t argc, gab_value argv[static argc]) {
  if (argc < 2) {
    gab_panic(gab, vm, "Invalid call to gab_lib_send");
    return;
  }

  // gab_negkeep(gab, argc, argv);
  // gab_ngciref(gab, gc, vm, 1, argc, argv);

  a_gab_value *result = gab_send(gab, NULL, NULL,
                                 (struct gab_send_argt){
                                     .flags = 0,
                                     .receiver = argv[0],
                                     .vmessage = argv[1],
                                     .len = argc - 2,
                                     .argv = argv + 2,
                                 });

  if (!result) {
    gab_panic(gab, vm, "Invalid send");
    return;
  }

  gab_nvmpush(vm, result->len, result->data);

  gab_ngcdref(gab, gc, vm, 1, result->len, result->data);

  a_gab_value_destroy(result);
}

void gab_lib_implements(struct gab_eg *gab, struct gab_gc *gc,
                        struct gab_vm *vm, size_t argc, gab_value argv[argc]) {
  gab_value type = gab_valtyp(gab, argv[0]);

  switch (argc) {
  case 2: {
    switch (gab_valknd(argv[1])) {
    case kGAB_MESSAGE: {
      bool implements = gab_msgat(argv[1], type) != gab_undefined;

      gab_vmpush(vm, gab_bool(implements));

      return;
    }
    case kGAB_SHAPE: {
      gab_value *keys = gab_shpdata(argv[1]);
      size_t len = gab_shplen(argv[1]);

      for (size_t i = 0; i < len; i++) {
        gab_value msgname = gab_valintos(gab, keys[i]);

        gab_value msg = gab_message(gab, msgname);

        if (gab_msgat(msg, type) == gab_undefined) {
          gab_vmpush(vm, gab_bool(false));
          return;
        }
      }

      gab_vmpush(vm, gab_bool(true));
      return;
    }
    default:
      break;
    }
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_implements");
    return;
  }
}

a_gab_value *gab_lib(struct gab_eg *gab, struct gab_gc *gc, struct gab_vm *vm) {
  const char *names[] = {
      "send",
      "implements?",
  };

  gab_value receivers[] = {
      gab_typ(gab, kGAB_UNDEFINED),
      gab_typ(gab, kGAB_UNDEFINED),
  };

  gab_value specs[] = {
      gab_sbuiltin(gab, "send", gab_lib_send),
      gab_sbuiltin(gab, "implements?", gab_lib_implements),
  };

  static_assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  static_assert(LEN_CARRAY(names) == LEN_CARRAY(specs));

  for (int i = 0; i < LEN_CARRAY(specs); i++) {

    gab_spec(gab, gc, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = receivers[i],
                 .specialization = specs[i],
             });
  }

  return NULL;
}
