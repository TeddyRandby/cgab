#include "gab.h"

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "&:len expects 1 argument");
  }

  gab_value result = gab_number(gab_strlen(argv[0]));

  gab_vmpush(gab_vm(gab), result);
  return nullptr;
};

static inline bool begins(const char *str, const char *pat, size_t offset) {
  size_t len = strlen(pat);

  if (strlen(str) < offset + len)
    return false;

  return !memcmp(str + offset, pat, len);
}

static inline bool ends(const char *str, const char *pat, size_t offset) {
  size_t len = strlen(pat);

  if (strlen(str) < offset + len)
    return false;

  return !memcmp(str + strlen(str) - offset - len, pat, len);
}
a_gab_value *gab_lib_ends(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_panic(gab, "&:ends? expects 1 string argument");
    }

    const char *pat = gab_valintocs(gab, argv[0]);
    const char *str = gab_valintocs(gab, argv[1]);

    gab_vmpush(gab_vm(gab), gab_bool(ends(str, pat, 0)));
    return nullptr;
  }

  case 3: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_panic(gab, "&:ends? expects 1 string argument");
    }

    if (gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_panic(gab, "&:ends? expects an optinal number argument");
    }
    const char *pat = gab_valintocs(gab, argv[0]);
    const char *str = gab_valintocs(gab, argv[1]);

    gab_vmpush(gab_vm(gab), gab_bool(ends(str, pat, gab_valton(argv[2]))));
    return nullptr;
  }
  }

  return nullptr;
}

a_gab_value *gab_lib_begins(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_panic(gab, "&:begins? expects 1 string argument");
    }

    const char *pat = gab_valintocs(gab, argv[0]);
    const char *str = gab_valintocs(gab, argv[1]);

    gab_vmpush(gab_vm(gab), gab_bool(begins(str, pat, 0)));
    return nullptr;
  }
  case 3: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_panic(gab, "&:begins? expects 1 string argument");
    }

    if (gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_panic(gab, "&:begins? expects an optinal number argument");
    }
    const char *pat = gab_valintocs(gab, argv[0]);
    const char *str = gab_valintocs(gab, argv[1]);

    gab_vmpush(gab_vm(gab), gab_bool(begins(str, pat, gab_valton(argv[2]))));
    return nullptr;
  }
  }
  return nullptr;
}

a_gab_value *gab_lib_is_digit(struct gab_triple gab, size_t argc,
                              gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "&:is_digit? expects 0 arguments");
  }

  int64_t index = argc == 1 ? 0 : gab_valton(argv[1]);

  if (index > gab_strlen(argv[0])) {
    return gab_panic(gab, "Index out of bounds");
  }

  if (index < 0) {
    // Go from the back
    index = gab_strlen(argv[0]) + index;

    if (index < 0) {
      return gab_panic(gab, "Index out of bounds");
    }
  }

  int byte = gab_valintocs(gab, argv[0])[index];

  gab_vmpush(gab_vm(gab), gab_bool(isdigit(byte)));
  return nullptr;
}

a_gab_value *gab_lib_to_byte(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "&:to_byte expects 0 arguments");
  }

  int64_t index = argc == 1 ? 0 : gab_valton(argv[1]);

  if (index > gab_strlen(argv[0])) {
    return gab_panic(gab, "Index out of bounds");
  }

  if (index < 0) {
    // Go from the back
    index = gab_strlen(argv[0]) + index;

    if (index < 0) {
      return gab_panic(gab, "Index out of bounds");
    }
  }

  char byte = gab_valintocs(gab, argv[0])[index];

  gab_vmpush(gab_vm(gab), gab_number(byte));
  return nullptr;
}

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
                        gab_value argv[argc]) {
  if (argc != 2 && gab_valkind(argv[1]) != kGAB_NUMBER) {
    return gab_panic(gab, "&:at expects 1 number argument");
  }

  long int index = gab_valton(argv[1]);

  if (index > gab_strlen(argv[0])) {
    return gab_panic(gab, "Index out of bounds");
  }

  if (index < 0) {
    // Go from the back
    index = gab_strlen(argv[0]) + index;
  }

  char byte = gab_valintocs(gab, argv[0])[index];

  gab_vmpush(gab_vm(gab), gab_nstring(gab, 1, &byte));
  return nullptr;
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (MAX(0, MIN(a, b)))

a_gab_value *gab_lib_slice(struct gab_triple gab, size_t argc,
                           gab_value argv[argc]) {
  const char *str = gab_valintocs(gab, argv[0]);

  uint64_t len = gab_strlen(argv[0]);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_panic(gab, "&:slice expects a number as the second argument");
    }

    double a = gab_valton(argv[1]);
    end = MIN(a, len);
    break;
  }

  case 3:
    if (gab_valkind(argv[1]) == kGAB_NUMBER) {
      start = MIN(gab_valton(argv[1]), len);
    } else if (argv[1] == gab_nil) {
      return gab_panic(gab, "&:slice expects a number as the second argument");
    }

    if (gab_valkind(argv[2]) == kGAB_NUMBER) {
      end = MIN(gab_valton(argv[2]), len);
    } else if (argv[2] == gab_nil) {
      return gab_panic(gab, "&:slice expects a number as the third argument");
    }
    break;

  default:
    return gab_panic(gab, "&:slice expects 2 or 3 arguments");
  }

  if (start >= end) {
    return gab_panic(gab, "&:slice expects the start to be before the end");
  }

  uint64_t size = end - start;

  gab_value res = gab_nstring(gab, size, str + start);

  gab_vmpush(gab_vm(gab), res);
  return nullptr;
}

a_gab_value *gab_lib_has(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc < 2) {
    return gab_panic(gab, "&:has? expects one argument");
  }

  const char *str = gab_valintocs(gab, argv[0]);
  const char *pat = gab_valintocs(gab, argv[1]);

  gab_vmpush(gab_vm(gab), gab_bool(strstr(str, pat)));
  return nullptr;
}

a_gab_value *gab_lib_string_into(struct gab_triple gab, size_t argc,
                                 gab_value argv[argc]) {
  gab_vmpush(gab_vm(gab), gab_arg(0));
  return nullptr;
}

a_gab_value *gab_lib_sigil_into(struct gab_triple gab, size_t argc,
                                gab_value argv[argc]) {
  gab_vmpush(gab_vm(gab), gab_strtosig(gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib_messages_into(struct gab_triple gab, size_t argc,
                                   gab_value argv[argc]) {
  gab_vmpush(gab_vm(gab), gab_strtomsg(gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib_new(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  if (argc < 2) {
    gab_vmpush(gab_vm(gab), gab_string(gab, ""));
    return nullptr;
  }

  gab_value str = gab_valintos(gab, gab_arg(1));

  if (argc == 2) {
    gab_vmpush(gab_vm(gab), str);
    return nullptr;
  }

  gab_gclock(gab);

  for (uint8_t i = 2; i < argc; i++) {
    gab_value curr = gab_valintos(gab, gab_arg(i));
    str = gab_strcat(gab, str, curr);
  }

  gab_vmpush(gab_vm(gab), str);
  gab_gcunlock(gab);
  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value string_type = gab_egtype(gab.eg, kGAB_STRING);

  struct gab_spec_argt specs[] = {
      {
          "slice",
          string_type,
          gab_snative(gab, "slice", gab_lib_slice),
      },
      {
          mGAB_CALL,
          gab_strtosig(string_type),
          gab_snative(gab, "strings.new", gab_lib_new),
      },
      {
          "strings.into",
          string_type,
          gab_snative(gab, "strings.into", gab_lib_string_into),
      },
      {
          "sigils.into",
          string_type,
          gab_snative(gab, "sigils.into", gab_lib_sigil_into),
      },
      {
          "messages.into",
          string_type,
          gab_snative(gab, "messages.into", gab_lib_messages_into),
      },
      {
          "len",
          string_type,
          gab_snative(gab, "len", gab_lib_len),
      },
      {
          "at",
          string_type,
          gab_snative(gab, "at", gab_lib_at),
      },
      {
          "ends?",
          string_type,
          gab_snative(gab, "ends?", gab_lib_ends),
      },
      {
          "begins?",
          string_type,
          gab_snative(gab, "begins?", gab_lib_begins),
      },
      {
          "to_byte",
          string_type,
          gab_snative(gab, "to_byte", gab_lib_to_byte),
      },
      {
          "is_digit?",
          string_type,
          gab_snative(gab, "is_digit?", gab_lib_is_digit),
      },
      {
          "has?",
          string_type,
          gab_snative(gab, "has?", gab_lib_has),
      }};

  gab_nspec(gab, sizeof(specs) / sizeof(struct gab_spec_argt), specs);

  return nullptr;
}
