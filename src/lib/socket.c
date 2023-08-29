#include "include/gab.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>

void gab_container_socket_cb(void *data) { shutdown((int64_t)data, SHUT_RDWR); }

void gab_lib_sock(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {

  int64_t result = socket(AF_INET, SOCK_STREAM, 0);

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "Could not open socket"));
    return;
  }

  gab_value res[2] = {gab_string(gab, "ok"),
                      gab_box(gab, vm,
                              (struct gab_box_argt){
                                  .type = gab_string(gab, "Socket"),
                                  .destructor = gab_container_socket_cb,
                                  .data = (void *)result,
                              })};

  gab_nvmpush(vm, 2, res);

  gab_gcdref(gab_vmgc(vm), vm, res[1]);

  return;
}

void gab_lib_bind(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {

  if (argc != 2 || gab_valknd(argv[0]) != kGAB_BOX ||
      gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_bind");
    return;
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  int32_t port = htons(gab_valton(argv[1]));

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_addr = {.s_addr = INADDR_ANY},
      .sin_port = port,
  };

  int32_t result = bind(socket, (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_BIND"));
  } else {
    gab_vmpush(vm, gab_string(gab, "ok"));
  }
}

void gab_lib_listen(gab_eg *gab, gab_vm *vm, size_t argc,
                    gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[0]) != kGAB_BOX ||
      gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "Invalid call to gab_lib_listen");
    return;
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  int32_t result = listen(socket, gab_valton(argv[1]));

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_LISTEN"));
  } else {
    gab_vmpush(vm, gab_string(gab, "ok"));
  }
}

void gab_lib_accept(gab_eg *gab, gab_vm *vm, size_t argc,
                    gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_accept");
    return;
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  int64_t result = accept(socket, &addr, &addrlen);

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_ACCEPT"));
    return;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_box(gab, vm,
              (struct gab_box_argt){
                  .type = gab_string(gab, "socket"),
                  .destructor = gab_container_socket_cb,
                  .data = (void *)result,
              }),
  };

  gab_nvmpush(vm, 2, res);

  gab_gcdref(gab_vmgc(vm), vm, res[1]);
}

void gab_lib_connect(gab_eg *gab, gab_vm *vm, size_t argc,
                     gab_value argv[argc]) {
  if (argc != 3 || gab_valknd(argv[2]) != kGAB_NUMBER ||
      gab_valknd(argv[1]) != kGAB_STRING) {
    gab_panic(gab, vm, "Invalid call to gab_lib_connect");
    return;
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  gab_obj_string *ip = GAB_VAL_TO_STRING(argv[1]);

  int32_t port = htons(gab_valton(argv[2]));

  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = port};

  if (inet_pton(AF_INET, (char *)ip->data, &addr.sin_addr) <= 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_CONVERT"));
    return;
  }

  int32_t result = connect(socket, (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_CONNECT"));
  } else {
    gab_vmpush(vm, gab_string(gab, "ok"));
  }
}

void gab_lib_receive(gab_eg *gab, gab_vm *vm, size_t argc,
                     gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_receive");
    return;
  }

  int8_t buffer[1024] = {0};

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  int32_t result = recv(socket, buffer, 1024, 0);

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_RECEIVE"));
  } else {
    gab_vmpush(vm, gab_nstring(gab, result, (char *)buffer));
  }
}

void gab_lib_send(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[1]) != kGAB_STRING) {
    gab_panic(gab, vm, "Invalid call to gab_lib_receive");
    return;
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  gab_obj_string *msg = GAB_VAL_TO_STRING(argv[1]);

  int32_t result = send(socket, msg->data, msg->len, 0);

  if (result < 0) {
    gab_vmpush(vm, gab_string(gab, "COULD_NOT_SEND"));
  } else {
    gab_vmpush(vm, gab_string(gab, "ok"));
  }
}

a_gab_value *gab_lib(gab_eg *gab, gab_vm *vm) {
  const char *names[] = {
      "socket", "bind", "listen", "accept", "receive", "send", "connect",
  };

  gab_value container_type = gab_string(gab, "Socket");

  gab_value types[] = {
      gab_nil,        container_type, container_type, container_type,
      container_type, container_type, container_type,
  };

  gab_value values[] = {
      gab_builtin(gab, "sock", gab_lib_sock),
      gab_builtin(gab, "bind", gab_lib_bind),
      gab_builtin(gab, "listen", gab_lib_listen),
      gab_builtin(gab, "accept", gab_lib_accept),
      gab_builtin(gab, "receive", gab_lib_receive),
      gab_builtin(gab, "send", gab_lib_send),
      gab_builtin(gab, "connect", gab_lib_connect),
  };

  assert(LEN_CARRAY(names) == LEN_CARRAY(types));
  assert(LEN_CARRAY(values) == LEN_CARRAY(types));
  assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (uint64_t i = 0; i < LEN_CARRAY(names); i++) {
    gab_spec(gab, vm,
             (struct gab_spec_argt){
                 .name = names[i],
                 .receiver = types[i],
                 .specialization = values[i],
             });
  }

  gab_ngcdref(gab_vmgc(vm), vm, LEN_CARRAY(values), values);

  return NULL;
}
