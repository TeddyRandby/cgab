#include "include/gab.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>

#define SOCKET_FAMILY "family"
#define SOCKET_TYPE "type"

#define SOCKET_BOX_TYPE "Socket"

void gab_container_socket_cb(void *data) { shutdown((int64_t)data, SHUT_RDWR); }

void gab_lib_sock(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  int domain, type;

  switch (argc) {
  case 2: {
    if (gab_valknd(argv[1]) != kGAB_RECORD) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    gab_value domain_val = gab_srecat(gab, argv[1], SOCKET_FAMILY);
    gab_value type_val = gab_srecat(gab, argv[1], SOCKET_TYPE);

    if (gab_valknd(domain_val) != kGAB_NUMBER) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    if (gab_valknd(type_val) != kGAB_NUMBER) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    domain = gab_valton(domain_val);
    type = gab_valton(type_val);

    break;
  }

  case 3: {
    domain = gab_valton(argv[1]);
    type = gab_valton(argv[2]);

    break;
  }

  default:
    gab_panic(gab, vm, "invalid_arguments");
    return;
  }

  int64_t sockfd = socket(domain, type, 0);

  if (sockfd < 0) {
    gab_vmpush(vm, gab_string(gab, "socket_open_failed"));
    return;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_box(gab, vm,
              (struct gab_box_argt){
                  .type = gab_string(gab, SOCKET_BOX_TYPE),
                  .destructor = gab_container_socket_cb,
                  .data = (void *)sockfd,
              }),
  };

  gab_nvmpush(vm, 2, res);

  gab_gcdref(gab_vmgc(vm), vm, res[1]);

  return;
}

void gab_lib_bind(gab_eg *gab, gab_vm *vm, size_t argc, gab_value argv[argc]) {
  switch (argc) {
  case 2: {
    gab_value config = argv[1];

    if (gab_valknd(config) != kGAB_RECORD) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    int sockfd = (intptr_t)gab_boxdata(argv[0]);

    gab_value family_value = gab_srecat(gab, config, SOCKET_FAMILY);

    if (gab_valknd(family_value) != kGAB_NUMBER) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    gab_value port_value = gab_srecat(gab, config, "port");

    if (gab_valknd(port_value) != kGAB_NUMBER) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    int family = gab_valton(family_value);

    int port = htons(gab_valton(port_value));

    int result = bind(sockfd,
                      (struct sockaddr *)(struct sockaddr_in[]){{
                          .sin_family = family,
                          .sin_addr = {.s_addr = INADDR_ANY},
                          .sin_port = port,
                      }},
                      sizeof(struct sockaddr_in));

    if (result < 0) {
      gab_vmpush(vm, gab_string(gab, "socket_bind_failed"));
      return;
    }

    gab_vmpush(vm, gab_string(gab, "ok"));
    return;
  }

  default:
    gab_panic(gab, vm, "invalid_arguments");
    return;
  }
}

void gab_lib_listen(gab_eg *gab, gab_vm *vm, size_t argc,
                    gab_value argv[argc]) {
  if (argc != 2 || gab_valknd(argv[0]) != kGAB_BOX ||
      gab_valknd(argv[1]) != kGAB_NUMBER) {
    gab_panic(gab, vm, "invalid_arguments");
    return;
  }

  int socket = (intptr_t)gab_boxdata(argv[0]);

  int result = listen(socket, gab_valton(argv[1]));

  if (result < 0)
    gab_vmpush(vm, gab_string(gab, "socket_listen_failed"));
  else
    gab_vmpush(vm, gab_string(gab, "ok"));
}

void gab_lib_accept(gab_eg *gab, gab_vm *vm, size_t argc,
                    gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "invalid_arguments");
    return;
  }

  int socket = (intptr_t)gab_boxdata(argv[0]);

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  int64_t connfd = accept(socket, &addr, &addrlen);

  if (connfd < 0) {
    gab_vmpush(vm, gab_string(gab, "socket_accept_failed"));
    return;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_box(gab, vm,
              (struct gab_box_argt){
                  .type = gab_string(gab, SOCKET_BOX_TYPE),
                  .destructor = gab_container_socket_cb,
                  .data = (void*)connfd,
              }),
  };

  gab_nvmpush(vm, 2, res);

  gab_gcdref(gab_vmgc(vm), vm, res[1]);
}

void gab_lib_connect(gab_eg *gab, gab_vm *vm, size_t argc,
                     gab_value argv[argc]) {
  switch (argc) {
  case 3: {
    if (gab_valknd(argv[2]) != kGAB_NUMBER) {
      gab_panic(gab, vm, "invalid_arguments");
      return;
    }

    int sockfd = (intptr_t)gab_boxdata(argv[0]);

    char *ip = gab_valtocs(gab, argv[1]);

    int port = htons(gab_valton(argv[2]));

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = port};

    int result = inet_pton(AF_INET, ip, &addr.sin_addr);
    free(ip);

    if (result <= 0) {
      gab_vmpush(vm, gab_string(gab, "inet_pton_failed"));
      return;
    }

    result = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    if (result < 0)
      gab_vmpush(vm, gab_string(gab, "socket_connect_failed"));
    else
      gab_vmpush(vm, gab_string(gab, "ok"));

    return;
  }
  default:
    gab_panic(gab, vm, "Invalid call to gab_lib_connect");
    return;
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
      gab_undefined,  container_type, container_type, container_type,
      container_type, container_type, container_type,
  };

  gab_value values[] = {
      gab_builtin(gab, "socket", gab_lib_sock),
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

  const char *constant_names[] = {
      "AF_INET",
      "SOCK_STREAM",
  };

  gab_value constant_values[] = {
      gab_number(AF_INET),
      gab_number(SOCK_STREAM),
  };

  gab_value constants = gab_srecord(gab, vm, LEN_CARRAY(constant_names),
                                    constant_names, constant_values);

  return a_gab_value_one(constants);
}
