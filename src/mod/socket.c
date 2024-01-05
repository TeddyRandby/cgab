#include "gab.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define SOCKET_FAMILY "family"
#define SOCKET_TYPE "type"

#define SOCKET_BOX_TYPE "Socket"

void gab_container_socket_cb(size_t len, unsigned char data[static len]) {
  shutdown((int64_t)data, SHUT_RDWR);
  close((int64_t)data);
}

a_gab_value *gab_lib_poll(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  int result, timeout;

  struct pollfd fd = {
      .fd = (intptr_t)gab_boxdata(argv[0]),
      .events = POLLIN,
  };

  switch (argc) {
  case 1:
    timeout = -1;
    break;

  case 2:
    if (gab_valkind(argv[1]) != kGAB_NUMBER) {
      return gab_panic(gab, "invalid_arguments");
    }

    timeout = gab_valton(argv[1]);
    break;

  default:
    return gab_panic(gab, "invalid_arguments");
  }

  result = poll(&fd, 1, timeout);

  if (result < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "poll_failed"));
    return NULL;
  }

  gab_vmpush(gab.vm, gab_number(fd.revents));
  return NULL;
}

a_gab_value *gab_lib_sock(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  int domain, type;

  switch (argc) {
  case 1: {
    domain = AF_INET;
    type = SOCK_STREAM;
    break;
  }
  case 2: {
    if (gab_valkind(argv[1]) != kGAB_RECORD) {
      return gab_panic(gab, "invalid_arguments");
    }

    gab_value domain_val = gab_srecat(gab, argv[1], SOCKET_FAMILY);
    gab_value type_val = gab_srecat(gab, argv[1], SOCKET_TYPE);

    if (gab_valkind(domain_val) != kGAB_NUMBER) {
      return gab_panic(gab, "invalid_arguments");
    }

    if (gab_valkind(type_val) != kGAB_NUMBER) {
      return gab_panic(gab, "invalid_arguments");
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
    return gab_panic(gab, "invalid_arguments");
  }

  int64_t sockfd = socket(domain, type, 0);

  if (sockfd < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "socket_open_failed"));
    return NULL;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_box(gab,
              (struct gab_box_argt){
                  .type = gab_string(gab, SOCKET_BOX_TYPE),
                  .destructor = gab_container_socket_cb,
                  .size = sizeof(int64_t),
                  .data = &sockfd,
              }),
  };

  gab_nvmpush(gab.vm, 2, res);

  return NULL;
}

a_gab_value *gab_lib_bind(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  int sockfd = (intptr_t)gab_boxdata(argv[0]);

  int family, port;

  switch (argc) {
  case 2: {
    gab_value config = argv[1];

    switch (gab_valkind(config)) {
    case kGAB_NUMBER:
      family = AF_INET;
      port = htons(gab_valton(config));
      goto fin;
    case kGAB_RECORD: {
      gab_value family_value = gab_srecat(gab, config, SOCKET_FAMILY);

      if (gab_valkind(family_value) != kGAB_NUMBER) {
        return gab_panic(gab, "invalid_arguments");
      }

      gab_value port_value = gab_srecat(gab, config, "port");

      if (gab_valkind(port_value) != kGAB_NUMBER) {
        return gab_panic(gab, "invalid_arguments");
      }

      family = gab_valton(family_value);

      port = htons(gab_valton(port_value));

      goto fin;
    }
    default:
      return gab_panic(gab, "invalid_arguments");
    }
  }

  default:
    return gab_panic(gab, "invalid_arguments");
  }

fin: {
  int result = bind(sockfd,
                    (struct sockaddr *)(struct sockaddr_in[]){{
                        .sin_family = family,
                        .sin_addr = {.s_addr = INADDR_ANY},
                        .sin_port = port,
                    }},
                    sizeof(struct sockaddr_in));

  if (result < 0)
    gab_vmpush(gab.vm, gab_string(gab, "socket_bind_failed"));
  else
    gab_vmpush(gab.vm, gab_string(gab, "ok"));
}
  return NULL;
}

a_gab_value *gab_lib_listen(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  if (argc != 2 || gab_valkind(argv[0]) != kGAB_BOX ||
      gab_valkind(argv[1]) != kGAB_NUMBER) {
    return gab_panic(gab, "invalid_arguments");
  }

  int socket = (intptr_t)gab_boxdata(argv[0]);

  int result = listen(socket, gab_valton(argv[1]));

  if (result < 0)
    gab_vmpush(gab.vm, gab_string(gab, "socket_listen_failed"));
  else
    gab_vmpush(gab.vm, gab_string(gab, "ok"));

  return NULL;
}

a_gab_value *gab_lib_accept(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "invalid_arguments");
  }

  int socket = (intptr_t)gab_boxdata(argv[0]);

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  int64_t connfd = accept(socket, &addr, &addrlen);

  if (connfd < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "socket_accept_failed"));
    return NULL;
  }

  gab_value res[2] = {
      gab_string(gab, "ok"),
      gab_box(gab,
              (struct gab_box_argt){
                  .type = gab_string(gab, SOCKET_BOX_TYPE),
                  .destructor = gab_container_socket_cb,
                  .data = (void *)connfd,
              }),
  };

  gab_nvmpush(gab.vm, 2, res);
  return NULL;
}

a_gab_value *gab_lib_connect(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  switch (argc) {
  case 3: {
    if (gab_valkind(argv[2]) != kGAB_NUMBER) {
      return gab_panic(gab, "invalid_arguments");
    }

    int sockfd = (intptr_t)gab_boxdata(argv[0]);

    const char *ip = gab_valintocs(gab, argv[1]);

    int port = htons(gab_valton(argv[2]));

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = port};

    int result = inet_pton(AF_INET, ip, &addr.sin_addr);

    if (result <= 0) {
      gab_vmpush(gab.vm, gab_string(gab, "inet_pton_failed"));
      return NULL;
    }

    result = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    if (result < 0)
      gab_vmpush(gab.vm, gab_string(gab, "socket_connect_failed"));
    else
      gab_vmpush(gab.vm, gab_string(gab, "ok"));

    return NULL;
  }
  default:
    return gab_panic(gab, "Invalid call to gab_lib_connect");
  }
}

a_gab_value *gab_lib_receive(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  if (argc != 1) {
    return gab_panic(gab, "Invalid call to gab_lib_receive");
  }

  int8_t buffer[1024] = {0};

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  int32_t result = recv(socket, buffer, 1024, 0);

  if (result < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "COULD_NOT_RECEIVE"));
  } else {
    gab_value vals[] = {
        gab_string(gab, "ok"),
        gab_nstring(gab, result, (char *)buffer),
    };
    gab_nvmpush(gab.vm, 2, vals);
  }

  return NULL;
}

a_gab_value *gab_lib_send(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  if (argc != 2 || gab_valkind(argv[1]) != kGAB_STRING) {
    return gab_panic(gab, "Invalid call to gab_lib_receive");
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  struct gab_obj_string *msg = GAB_VAL_TO_STRING(argv[1]);

  int32_t result = send(socket, msg->data, msg->len, 0);

  if (result < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "COULD_NOT_SEND"));
  } else {
    gab_vmpush(gab.vm, gab_string(gab, "ok"));
  }

  return NULL;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  const char *names[] = {
      "socket", "bind", "listen", "accept", "recv", "send", "connect",
  };

  gab_value container_type = gab_string(gab, "Socket");

  gab_value receivers[] = {
      gab_undefined,  container_type, container_type, container_type,
      container_type, container_type, container_type,
  };

  gab_value values[] = {
      gab_snative(gab, "socket", gab_lib_sock),
      gab_snative(gab, "bind", gab_lib_bind),
      gab_snative(gab, "listen", gab_lib_listen),
      gab_snative(gab, "accept", gab_lib_accept),
      gab_snative(gab, "receive", gab_lib_receive),
      gab_snative(gab, "send", gab_lib_send),
      gab_snative(gab, "connect", gab_lib_connect),
  };

  assert(LEN_CARRAY(names) == LEN_CARRAY(receivers));
  assert(LEN_CARRAY(values) == LEN_CARRAY(receivers));
  assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (uint64_t i = 0; i < LEN_CARRAY(names); i++) {
    gab_spec(gab, (struct gab_spec_argt){
                      .name = names[i],
                      .receiver = receivers[i],
                      .specialization = values[i],
                  });
  }

  const char *constant_names[] = {
      "AF_INET",
      "SOCK_STREAM",
  };

  gab_value constant_values[] = {
      gab_number(AF_INET),
      gab_number(SOCK_STREAM),
  };

  gab_value constants = gab_srecord(gab, LEN_CARRAY(constant_names),
                                    constant_names, constant_values);

  return a_gab_value_one(constants);
}
