#include "include/core.h"
#include "include/gab.h"

void gab_lib_send(struct gab_triple gab, size_t argc,
                  gab_value argv[static argc]) {
  if (argc < 2) {
    gab_panic(gab, "Invalid call to gab_lib_send");
    return;
  }

  a_gab_value *result = gab_send(gab, (struct gab_send_argt){
                                          .flags = 0,
                                          .receiver = argv[0],
                                          .vmessage = argv[1],
                                          .len = argc - 2,
                                          .argv = argv + 2,
                                      });

  if (!result) {
    gab_panic(gab, "Invalid send");
    return;
  }

  gab_nvmpush(gab.vm, result->len, result->data);

  a_gab_value_destroy(result);
}

void gab_lib_implements(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  gab_value type = gab_valtyp(gab.eg, argv[0]);

  switch (argc) {
  case 2: {
    switch (gab_valknd(argv[1])) {
    case kGAB_MESSAGE: {
      bool implements = gab_msgat(argv[1], type) != gab_undefined;

      gab_vmpush(gab.vm, gab_bool(implements));

      return;
    }
    case kGAB_SHAPE: {
      gab_value *keys = gab_shpdata(argv[1]);
      size_t len = gab_shplen(argv[1]);

      for (size_t i = 0; i < len; i++) {
        gab_value msgname = gab_valintos(gab, keys[i]);

        gab_value msg = gab_message(gab, msgname);

        if (gab_msgat(msg, type) == gab_undefined) {
          gab_vmpush(gab.vm, gab_bool(false));
          return;
        }
      }

      gab_vmpush(gab.vm, gab_bool(true));
      return;
    }
    default:
      break;
    }
  }
  default:
    gab_panic(gab, "Invalid call to gab_lib_implements");
    return;
  }
}

a_gab_value *gab_lib(struct gab_triple gab) {
  struct gab_spec_argt specs[] = {
      {
          "implements?",
          gab_typ(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "implements?", gab_lib_implements),
      },
      {
          "send",
          gab_typ(gab.eg, kGAB_UNDEFINED),
          gab_sbuiltin(gab, "send", gab_lib_send),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return NULL;
}
