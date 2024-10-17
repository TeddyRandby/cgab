#include "gab.h"
#include <ctype.h>

static inline bool instr(char c, const char *set) {
  while (*set != '\0')
    if (c == *set++)
      return true;

  return false;
}

a_gab_value *gab_lib_trim(struct gab_triple gab, uint64_t argc,
                          gab_value argv[argc]) {
  gab_value str = gab_arg(0);
  gab_value trimset = gab_arg(1);

  const char *cstr = gab_strdata(&str);
  const char *ctrimset = nullptr;
  uint64_t cstrlen = gab_strlen(str);

  if (trimset == gab_nil)
    trimset = gab_string(gab, "\n\t ");

  if (gab_valkind(trimset) != kGAB_STRING)
    return gab_pktypemismatch(gab, trimset, kGAB_STRING);

  if (cstrlen == 0) {
    gab_vmpush(gab_vm(gab), str);
    return nullptr;
  }

  ctrimset = gab_strdata(&trimset);

  const char *front = cstr;
  const char *back = cstr + cstrlen - 1;

  while (instr(*front, ctrimset) && front < back)
    front++;

  while (instr(*back, ctrimset) && back > front)
    back--;

  uint64_t result_len = back - front + 1;

  gab_vmpush(gab_vm(gab), gab_nstring(gab, result_len, front));
  return nullptr;
}

a_gab_value *gab_lib_split(struct gab_triple gab, uint64_t argc,
                           gab_value argv[argc]) {
  gab_value str = gab_arg(0);
  gab_value sep = gab_arg(1);

  if (gab_valkind(sep) != kGAB_STRING)
    return gab_pktypemismatch(gab, sep, kGAB_STRING);

  uint64_t cstr_len = gab_strlen(str);
  uint64_t csep_len = gab_strlen(sep);

  if (cstr_len == 0 || csep_len == 0)
    return nullptr;

  const char *cstr = gab_strdata(&str);
  const char *csep = gab_strdata(&sep);
  const char sepstart = csep[0];

  uint64_t offset = 0, begin = 0;
  while (offset + csep_len < cstr_len) {

    // Quick check to see if we should try memcmp
    if (cstr[offset] == sepstart) {
      // Memcmp to test for full sep match
      if (!memcmp(cstr + offset, csep, csep_len)) {
        // Full match found - push a string
        gab_vmpush(gab_vm(gab), gab_nstring(gab, offset - begin, cstr + begin));
        begin = offset + csep_len;
        offset = begin;
      }
    }

    offset++;
  }

  gab_vmpush(gab_vm(gab), gab_nstring(gab, cstr_len - begin, cstr + begin));

  return nullptr;
}

a_gab_value *gab_lib_len(struct gab_triple gab, uint64_t argc,
                         gab_value argv[argc]) {
  if (argc != 1) {
    return gab_fpanic(gab, "&:len expects 1 argument");
  }

  gab_value result = gab_number(gab_strlen(argv[0]));

  gab_vmpush(gab_vm(gab), result);
  return nullptr;
};

static _Atomic uint64_t gab_eval_nth = -1;

a_gab_value *gab_lib_gabeval(struct gab_triple gab, uint64_t argc,
                             gab_value argv[argc]) {
  gab_value source = gab_arg(0);

  gab_eval_nth++;

  char buff[8];

  snprintf(buff, 8, "eval:%lu", gab_eval_nth);

  a_gab_value *result = gab_exec(gab, (struct gab_exec_argt){
                                          .flags = gab.flags,
                                          .source = gab_strdata(&source),
                                          .name = buff,
                                      });

  if (!result)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, "Failed to compile"));
  else
    gab_nvmpush(gab_vm(gab), result->len, result->data);

  return nullptr;
};

static inline bool begins(const char *str, const char *pat, uint64_t offset) {
  uint64_t len = strlen(pat);

  if (strlen(str) < offset + len)
    return false;

  return !memcmp(str + offset, pat, len);
}

static inline bool ends(const char *str, const char *pat, uint64_t offset) {
  uint64_t len = strlen(pat);

  if (strlen(str) < offset + len)
    return false;

  return !memcmp(str + strlen(str) - offset - len, pat, len);
}

a_gab_value *gab_lib_blank(struct gab_triple gab, uint64_t argc,
                           gab_value argv[argc]) {
  gab_value str = gab_arg(0);

  if (gab_valkind(str) != kGAB_STRING)
    return gab_pktypemismatch(gab, str, kGAB_STRING);

  const char *cstr = gab_strdata(&str);

  while (*cstr) {
    if (!isspace(*cstr)) {
      gab_vmpush(gab_vm(gab), gab_false);
      return nullptr;
    }

    cstr++;
  }

  gab_vmpush(gab_vm(gab), gab_true);
  return nullptr;
}

a_gab_value *gab_lib_ends(struct gab_triple gab, uint64_t argc,
                          gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_fpanic(gab, "&:ends? expects 1 string argument");
    }

    const char *str = gab_strdata(argv + 0);
    const char *pat = gab_strdata(argv + 1);

    gab_vmpush(gab_vm(gab), gab_bool(ends(str, pat, 0)));
    return nullptr;
  }

  case 3: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_fpanic(gab, "&:ends? expects 1 string argument");
    }

    if (gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_fpanic(gab, "&:ends? expects an optinal number argument");
    }
    const char *pat = gab_strdata(argv + 0);
    const char *str = gab_strdata(argv + 1);

    gab_vmpush(gab_vm(gab), gab_bool(ends(str, pat, gab_valton(argv[2]))));
    return nullptr;
  }
  }

  return nullptr;
}

a_gab_value *gab_lib_begins(struct gab_triple gab, uint64_t argc,
                            gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_fpanic(gab, "&:begins? expects 1 string argument");
    }

    const char *pat = gab_strdata(argv + 0);
    const char *str = gab_strdata(argv + 1);

    gab_vmpush(gab_vm(gab), gab_bool(begins(str, pat, 0)));
    return nullptr;
  }
  case 3: {
    if (gab_valkind(argv[1]) != kGAB_STRING) {
      return gab_fpanic(gab, "&:begins? expects 1 string argument");
    }

    if (gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_fpanic(gab, "&:begins? expects an optinal number argument");
    }
    const char *pat = gab_strdata(argv + 0);
    const char *str = gab_strdata(argv + 1);

    gab_vmpush(gab_vm(gab), gab_bool(begins(str, pat, gab_valton(argv[2]))));
    return nullptr;
  }
  }
  return nullptr;
}

a_gab_value *gab_lib_is_digit(struct gab_triple gab, uint64_t argc,
                              gab_value argv[argc]) {
  if (argc != 1) {
    return gab_fpanic(gab, "&:is_digit? expects 0 arguments");
  }

  int64_t index = argc == 1 ? 0 : gab_valton(argv[1]);

  if (index > gab_strlen(argv[0])) {
    return gab_fpanic(gab, "Index out of bounds");
  }

  if (index < 0) {
    // Go from the back
    index = gab_strlen(argv[0]) + index;

    if (index < 0) {
      return gab_fpanic(gab, "Index out of bounds");
    }
  }

  int byte = gab_strdata(argv + 0)[index];

  gab_vmpush(gab_vm(gab), gab_bool(isdigit(byte)));
  return nullptr;
}

a_gab_value *gab_lib_to_byte(struct gab_triple gab, uint64_t argc,
                             gab_value argv[argc]) {
  if (argc != 1) {
    return gab_fpanic(gab, "&:to_byte expects 0 arguments");
  }

  int64_t index = argc == 1 ? 0 : gab_valton(argv[1]);

  if (index > gab_strlen(argv[0])) {
    return gab_fpanic(gab, "Index out of bounds");
  }

  if (index < 0) {
    // Go from the back
    index = gab_strlen(argv[0]) + index;

    if (index < 0) {
      return gab_fpanic(gab, "Index out of bounds");
    }
  }

  char byte = gab_strdata(argv + 0)[index];

  gab_vmpush(gab_vm(gab), gab_number(byte));
  return nullptr;
}

a_gab_value *gab_lib_at(struct gab_triple gab, uint64_t argc,
                        gab_value argv[argc]) {
  if (argc != 2 && gab_valkind(argv[1]) != kGAB_NUMBER) {
    return gab_fpanic(gab, "&:at expects 1 number argument");
  }

  long int index = gab_valton(argv[1]);

  if (index > gab_strlen(argv[0])) {
    return gab_fpanic(gab, "Index out of bounds");
  }

  if (index < 0) {
    // Go from the back
    index = gab_strlen(argv[0]) + index;
  }

  char byte = gab_strdata(argv + 0)[index];

  gab_vmpush(gab_vm(gab), gab_nstring(gab, 1, &byte));
  return nullptr;
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (MAX(0, MIN(a, b)))

a_gab_value *gab_lib_slice(struct gab_triple gab, uint64_t argc,
                           gab_value argv[argc]) {
  const char *str = gab_strdata(argv + 0);

  uint64_t len = gab_strlen(argv[0]);
  uint64_t start = 0, end = len;

  switch (argc) {
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_fpanic(gab, "&:slice expects a number as the second argument");
    }

    double a = gab_valton(argv[1]);
    end = MIN(a, len);
    break;
  }

  case 3:
    if (gab_valkind(argv[1]) == kGAB_NUMBER) {
      start = MIN(gab_valton(argv[1]), len);
    } else if (argv[1] == gab_nil) {
      return gab_fpanic(gab, "&:slice expects a number as the second argument");
    }

    if (gab_valkind(argv[2]) == kGAB_NUMBER) {
      end = MIN(gab_valton(argv[2]), len);
    } else if (argv[2] == gab_nil) {
      return gab_fpanic(gab, "&:slice expects a number as the third argument");
    }
    break;

  default:
    return gab_fpanic(gab, "&:slice expects 2 or 3 arguments");
  }

  if (start >= end) {
    return gab_fpanic(gab, "&:slice expects the start to be before the end");
  }

  uint64_t size = end - start;

  gab_value res = gab_nstring(gab, size, str + start);

  gab_vmpush(gab_vm(gab), res);
  return nullptr;
}

a_gab_value *gab_lib_has(struct gab_triple gab, uint64_t argc,
                         gab_value argv[argc]) {
  if (argc < 2) {
    return gab_fpanic(gab, "&:has? expects one argument");
  }

  const char *str = gab_strdata(argv + 0);
  const char *pat = gab_strdata(argv + 1);

  gab_vmpush(gab_vm(gab), gab_bool(strstr(str, pat)));
  return nullptr;
}

a_gab_value *gab_lib_string_into(struct gab_triple gab, uint64_t argc,
                                 gab_value argv[argc]) {
  gab_vmpush(gab_vm(gab), gab_valintos(gab, gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib_sigil_into(struct gab_triple gab, uint64_t argc,
                                gab_value argv[argc]) {
  gab_vmpush(gab_vm(gab), gab_strtosig(gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib_messages_into(struct gab_triple gab, uint64_t argc,
                                   gab_value argv[argc]) {
  gab_vmpush(gab_vm(gab), gab_strtomsg(gab_arg(0)));
  return nullptr;
}

a_gab_value *gab_lib_new(struct gab_triple gab, uint64_t argc,
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

a_gab_value *gab_lib_numbers_into(struct gab_triple gab, uint64_t argc,
                                  gab_value argv[argc]) {
  const char *str = gab_strdata(argv + 0);

  gab_value res = gab_number(strtod(str, nullptr));

  gab_vmpush(gab_vm(gab), res);
  return nullptr;
};

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value string_type = gab_type(gab, kGAB_STRING);

  gab_def(gab,
          {
              gab_message(gab, "slice"),
              string_type,
              gab_snative(gab, "slice", gab_lib_slice),
          },
          {
              gab_message(gab, mGAB_CALL),
              gab_strtosig(string_type),
              gab_snative(gab, "strings.new", gab_lib_new),
          },
          {
              gab_message(gab, "strings.into"),
              gab_undefined,
              gab_snative(gab, "strings.into", gab_lib_string_into),
          },
          {
              gab_message(gab, "sigils.into"),
              string_type,
              gab_snative(gab, "sigils.into", gab_lib_sigil_into),
          },
          {
              gab_message(gab, "messages.into"),
              string_type,
              gab_snative(gab, "messages.into", gab_lib_messages_into),
          },
          {
              gab_message(gab, "numbers.into"),
              string_type,
              gab_snative(gab, "numbers.into", gab_lib_numbers_into),
          },
          {
              gab_message(gab, "len"),
              string_type,
              gab_snative(gab, "len", gab_lib_len),
          },
          {
              gab_message(gab, "at"),
              string_type,
              gab_snative(gab, "at", gab_lib_at),
          },
          {
              gab_message(gab, "trim"),
              string_type,
              gab_snative(gab, "trim", gab_lib_trim),
          },
          {
              gab_message(gab, "split"),
              string_type,
              gab_snative(gab, "split", gab_lib_split),
          },
          {
              gab_message(gab, "blank?"),
              string_type,
              gab_snative(gab, "blank?", gab_lib_blank),
          },
          {
              gab_message(gab, "ends?"),
              string_type,
              gab_snative(gab, "ends?", gab_lib_ends),
          },
          {
              gab_message(gab, "begins?"),
              string_type,
              gab_snative(gab, "begins?", gab_lib_begins),
          },
          {
              gab_message(gab, "to_byte"),
              string_type,
              gab_snative(gab, "to_byte", gab_lib_to_byte),
          },
          {
              gab_message(gab, "is_digit?"),
              string_type,
              gab_snative(gab, "is_digit?", gab_lib_is_digit),
          },
          {
              gab_message(gab, "has?"),
              string_type,
              gab_snative(gab, "has?", gab_lib_has),
          },
          {
              gab_message(gab, "gab.eval"),
              string_type,
              gab_snative(gab, "gab.eval", gab_lib_gabeval),
          });

  return nullptr;
}