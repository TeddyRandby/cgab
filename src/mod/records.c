#include "gab.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

a_gab_value *gab_reclib_at(struct gab_triple gab, uint64_t argc,
                           gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value val = gab_recat(rec, key);

  if (val == gab_undefined)
    gab_vmpush(gab_vm(gab), gab_none);
  else
    gab_vmpush(gab_vm(gab), gab_ok, val);

  return nullptr;
}

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b) (MAX(0, MIN(a, b)))

a_gab_value *gab_reclib_slice(struct gab_triple gab, uint64_t argc,
                              gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  uint64_t len = gab_reclen(argv[0]);
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

  if (start > end) {
    return gab_fpanic(gab, "&:slice expects the start to be before the end");
  }

  uint64_t size = end - start;
  gab_value vs[size];
  for (uint64_t i = 0; i < size; i++) {
    vs[i] = gab_uvrecat(rec, start + i);
  }

  gab_vmpush(gab_vm(gab), gab_list(gab, size, vs));

  return nullptr;
}

a_gab_value *gab_reclib_push(struct gab_triple gab, uint64_t argc,
                             gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  uint64_t start = gab_reclen(rec) - 1;

  for (uint64_t i = 1; i < argc; i++) {
    gab_value key = gab_number(start + i);
    gab_value val = gab_arg(i);
    rec = gab_recput(gab, rec, key, val);
  }

  gab_vmpush(gab_vm(gab), rec);

  return nullptr;
}

a_gab_value *gab_reclib_put(struct gab_triple gab, uint64_t argc,
                            gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);
  gab_value val = gab_arg(2);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_vmpush(gab_vm(gab), gab_recput(gab, rec, key, val));

  return nullptr;
}

a_gab_value *gab_reclib_is_list(struct gab_triple gab, uint64_t argc,
                                gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value shp = gab_recshp(rec);
  uint64_t len = gab_shplen(shp);

  if (len == 0) {
    gab_vmpush(gab_vm(gab), gab_false);
    return nullptr;
  }

  for (uint64_t i = 0; i < len; i++) {
    if (gab_valkind(gab_ushpat(shp, i)) != kGAB_NUMBER) {
      gab_vmpush(gab_vm(gab), gab_false);
      return nullptr;
    }
  }

  gab_vmpush(gab_vm(gab), gab_true);
  return nullptr;
}

gab_value doputvia(struct gab_triple gab, gab_value rec, gab_value val,
                   uint64_t len, gab_value path[len]) {
  gab_value key = path[0];

  if (len == 1)
    return gab_recput(gab, rec, key, val);

  gab_value subrec = gab_recat(rec, key);

  if (subrec == gab_undefined)
    subrec = gab_record(gab, 0, 0, path, path);

  if (gab_valkind(subrec) != kGAB_RECORD)
    return gab_undefined;

  gab_value subval = doputvia(gab, subrec, val, len - 1, path + 1);

  if (subval == gab_undefined)
    return gab_undefined;

  return gab_recput(gab, rec, key, subval);
}

a_gab_value *gab_reclib_putvia(struct gab_triple gab, uint64_t argc,
                               gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value val = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value result = doputvia(gab, rec, val, argc - 2, argv + 2);

  if (result == gab_undefined)
    return gab_fpanic(gab, "Invalid path for $ on $",
                      gab_message(gab, "putvia"), rec);

  gab_vmpush(gab_vm(gab), result);

  return nullptr;
}

a_gab_value *gab_reclib_len(struct gab_triple gab, uint64_t argc,
                            gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_vmpush(gab_vm(gab), gab_number(gab_reclen(rec)));

  return nullptr;
}

a_gab_value *gab_reclib_strings_into(struct gab_triple gab, uint64_t argc,
                                     gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  FILE *tmp = tmpfile();

  if (tmp == nullptr)
    return gab_fpanic(gab, "Failed: $", gab_string(gab, strerror(errno)));

  int bytes = gab_fvalinspect(tmp, rec, 3);

  rewind(tmp);

  uint8_t b[bytes];
  if (fread(b, sizeof(uint8_t), bytes, tmp) != bytes)
    return gab_fpanic(gab, "Failed: $", gab_string(gab, strerror(errno)));

  if (fclose(tmp))
    return gab_fpanic(gab, "Failed: $", gab_string(gab, strerror(errno)));

  gab_value str = gab_nstring(gab, bytes, (char *)b);

  gab_vmpush(gab_vm(gab), str);

  return nullptr;
}

a_gab_value *gab_reclib_seqinit(struct gab_triple gab, uint64_t argc,
                                gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  uint64_t len = gab_reclen(rec);

  if (len == 0) {
    gab_vmpush(gab_vm(gab), gab_none);
    return nullptr;
  }

  gab_value key = gab_ukrecat(rec, 0);
  gab_value val = gab_uvrecat(rec, 0);

  gab_vmpush(gab_vm(gab), gab_ok, key, val, key);
  return nullptr;
}

a_gab_value *gab_reclib_seqnext(struct gab_triple gab, uint64_t argc,
                                gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value old_key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  uint64_t len = gab_reclen(rec);

  if (len == 0)
    goto fin;

  uint64_t i = gab_recfind(rec, old_key);

  if (i == -1 || i + 1 == len)
    goto fin;

  gab_value key = gab_ukrecat(rec, i + 1);
  gab_value val = gab_uvrecat(rec, i + 1);

  gab_vmpush(gab_vm(gab), gab_ok, key, val, key);
  return nullptr;

fin:
  gab_vmpush(gab_vm(gab), gab_none);
  return nullptr;
}
