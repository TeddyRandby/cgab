#include "gab.h"
#include <errno.h>
#include <stdio.h>

a_gab_value *gab_lib_at(struct gab_triple gab, size_t argc,
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

a_gab_value *gab_lib_del(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value new_rec = gab_recdel(gab, rec, key);

  gab_vmpush(gab_vm(gab), new_rec);

  return nullptr;
}

a_gab_value *gab_lib_push(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  size_t start = gab_reclen(rec) - 1;

  for (size_t i = 1; i < argc; i++) {
    gab_value key = gab_number(start + i);
    gab_value val = gab_arg(i);
    rec = gab_recput(gab, rec, key, val);
  }

  gab_vmpush(gab_vm(gab), rec);

  return nullptr;
}

a_gab_value *gab_lib_put(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value key = gab_arg(1);
  gab_value val = gab_arg(2);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_vmpush(gab_vm(gab), gab_recput(gab, rec, key, val));

  return nullptr;
}

a_gab_value *gab_lib_islist(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_value shp = gab_recshp(rec);
  size_t len = gab_shplen(shp);

  if (len == 0) {
    gab_vmpush(gab_vm(gab), gab_false);
    return nullptr;
  }

  for (size_t i = 0; i < len; i++) {
    if (gab_valkind(gab_ushpat(shp, i)) != kGAB_NUMBER) {
      gab_vmpush(gab_vm(gab), gab_false);
      return nullptr;
    }
  }

  gab_vmpush(gab_vm(gab), gab_true);
  return nullptr;
}

gab_value doputvia(struct gab_triple gab, gab_value rec, gab_value val,
                   size_t len, gab_value path[len]) {
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

a_gab_value *gab_lib_putvia(struct gab_triple gab, size_t argc,
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

a_gab_value *gab_lib_len(struct gab_triple gab, size_t argc,
                         gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  gab_vmpush(gab_vm(gab), gab_number(gab_reclen(rec)));

  return nullptr;
}

a_gab_value *gab_lib_strings_into(struct gab_triple gab, size_t argc,
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

a_gab_value *gab_lib_init(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value rec = gab_arg(0);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  size_t len = gab_reclen(rec);

  if (len == 0) {
    gab_vmpush(gab_vm(gab), gab_none);
    return nullptr;
  }

  gab_value key = gab_ukrecat(rec, 0);
  gab_value val = gab_uvrecat(rec, 0);

  gab_vmpush(gab_vm(gab), gab_ok, key, val, key);
  return nullptr;
}

a_gab_value *gab_lib_next(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value rec = gab_arg(0);
  gab_value old_key = gab_arg(1);

  if (gab_valkind(rec) != kGAB_RECORD)
    return gab_pktypemismatch(gab, rec, kGAB_RECORD);

  size_t len = gab_reclen(rec);

  if (len == 0)
    goto fin;

  size_t i = gab_recfind(rec, old_key);

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

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value rec_t = gab_type(gab, kGAB_RECORD);

  gab_def(gab,
          {
              gab_message(gab, "list?"),
              rec_t,
              gab_snative(gab, "gab.list?", gab_lib_islist),
          },
          {
              gab_message(gab, "put"),
              rec_t,
              gab_snative(gab, "gab.put", gab_lib_put),
          },
          {
              gab_message(gab, "del"),
              rec_t,
              gab_snative(gab, "gab.del", gab_lib_del),
          },
          {
              gab_message(gab, "at"),
              rec_t,
              gab_snative(gab, "gab.at", gab_lib_at),
          },
          {
              gab_message(gab, "push"),
              rec_t,
              gab_snative(gab, "gab.push", gab_lib_push),
          },
          {
              gab_message(gab, "seqs.next"),
              rec_t,
              gab_snative(gab, "gab.next", gab_lib_next),
          },
          {
              gab_message(gab, "seqs.init"),
              rec_t,
              gab_snative(gab, "gab.init", gab_lib_init),
          },
          {
              gab_message(gab, "len"),
              rec_t,
              gab_snative(gab, "gab.len", gab_lib_len),
          },
          {
              gab_message(gab, "put_via"),
              rec_t,
              gab_snative(gab, "gab.put_via", gab_lib_putvia),
          },
          {
              gab_message(gab, "strings.into"),
              rec_t,
              gab_snative(gab, "gab.strings.into", gab_lib_strings_into),
          });

  return a_gab_value_one(rec_t);
}
