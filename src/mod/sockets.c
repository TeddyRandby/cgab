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

void gab_container_socket_cb(size_t len, char data[static len]) {
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
    return nullptr;
  }

  gab_vmpush(gab.vm, gab_number(fd.revents));
  return nullptr;
}

const char *sock_config_keys[] = {SOCKET_FAMILY, SOCKET_TYPE};

a_gab_value *gab_lib_sock(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  int domain = AF_INET, type = SOCK_STREAM;

  switch (argc) {
  case 1:
    break;

  case 2: {
    if (gab_valkind(argv[1]) != kGAB_RECORD)
      return gab_ptypemismatch(gab, argv[1],
                               gab_sshape(gab, 1, 2, sock_config_keys));

    gab_value domain_val = gab_srecat(gab, argv[1], SOCKET_FAMILY);
    gab_value type_val = gab_srecat(gab, argv[1], SOCKET_TYPE);

    if (gab_valkind(domain_val) != kGAB_NUMBER)
      return gab_panic(gab, "invalid_arguments");

    if (gab_valkind(type_val) != kGAB_NUMBER)
      return gab_panic(gab, "invalid_arguments");

    domain = gab_valton(domain_val);
    type = gab_valton(type_val);

    break;
  }

  default: {
    gab_value domain_val = gab_arg(1);
    gab_value type_val = gab_arg(2);

    if (gab_valkind(domain_val) != kGAB_NUMBER)
      return gab_pktypemismatch(gab, domain_val, kGAB_NUMBER);

    if (gab_valkind(type_val) != kGAB_NUMBER)
      return gab_pktypemismatch(gab, type_val, kGAB_NUMBER);

    domain = gab_valton(argv[1]);
    type = gab_valton(argv[2]);

    break;
  }
  }

  int64_t sockfd = socket(domain, type, 0);

  if (sockfd < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "socket_open_failed"));
    return nullptr;
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

  return nullptr;
}

a_gab_value *gab_lib_bind(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  int sockfd = (intptr_t)gab_boxdata(argv[0]);

  int family, port;

  gab_value config = gab_arg(1);

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
    return gab_pktypemismatch(gab, config, kGAB_NUMBER);
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
    gab_vmpush(gab.vm, gab_string(gab, "BIND_FAILED"));
  else
    gab_vmpush(gab.vm, gab_string(gab, "ok"));
}
  return nullptr;
}

a_gab_value *gab_lib_listen(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  gab_value port = gab_arg(1);

  if (gab_valkind(port) != kGAB_NUMBER)
    return gab_pktypemismatch(gab, port, kGAB_NUMBER);

  int socket = (intptr_t)gab_boxdata(argv[0]);

  int result = listen(socket, gab_valton(port));

  if (result < 0)
    gab_vmpush(gab.vm, gab_string(gab, "LISTEN_FAILED"));
  else
    gab_vmpush(gab.vm, gab_string(gab, "ok"));

  return nullptr;
}

a_gab_value *gab_lib_accept(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  int socket = (intptr_t)gab_boxdata(argv[0]);

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  int64_t connfd = accept(socket, &addr, &addrlen);

  if (connfd < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "ACCEPT_FAILED"));
    return nullptr;
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
  return nullptr;
}

a_gab_value *gab_lib_connect(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  gab_value host = gab_arg(1);
  gab_value port = gab_arg(2);

  if (gab_valkind(host) != kGAB_STRING) {
    return gab_pktypemismatch(gab, host, kGAB_STRING);
  }

  if (gab_valkind(port) != kGAB_NUMBER) {
    return gab_pktypemismatch(gab, port, kGAB_NUMBER);
  }

  int sockfd = (intptr_t)gab_boxdata(argv[0]);

  const char *ip = gab_valintocs(gab, argv[1]);

  int cport = htons(gab_valton(argv[2]));

  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = cport};

  int result = inet_pton(AF_INET, ip, &addr.sin_addr);

  if (result <= 0) {
    gab_vmpush(gab.vm, gab_string(gab, "INET_PTON_FAILED"));
    return nullptr;
  }

  result = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0)
    gab_vmpush(gab.vm, gab_string(gab, "CONNECT_FAILED"));
  else
    gab_vmpush(gab.vm, gab_string(gab, "ok"));

  return nullptr;
}

a_gab_value *gab_lib_receive(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  int8_t buffer[1024] = {0};

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  int32_t result = recv(socket, buffer, 1024, 0);

  if (result < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "RECEIVE_FAILED"));
  } else {
    gab_value vals[] = {
        gab_string(gab, "ok"),
        gab_nstring(gab, result, (char *)buffer),
    };
    gab_nvmpush(gab.vm, 2, vals);
  }

  return nullptr;
}

a_gab_value *gab_lib_send(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value msg = gab_arg(1);

  if (gab_valkind(msg) != kGAB_STRING) {
    return gab_ptypemismatch(gab, msg, gab_egtype(gab.eg, kGAB_STRING));
  }

  int64_t socket = (int64_t)GAB_VAL_TO_BOX(argv[0])->data;

  int32_t result = send(socket, gab_strdata(&msg), gab_strlen(msg), 0);

  if (result < 0) {
    gab_vmpush(gab.vm, gab_string(gab, "SEND_FAILED"));
  } else {
    gab_vmpush(gab.vm, gab_string(gab, "ok"));
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value container_type = gab_string(gab, "gab.socket");

  struct gab_spec_argt specs[] = {
      {
          "gab.socket",
          gab_undefined,
          gab_snative(gab, "gab.socket", gab_lib_sock),
      },
      {
          "bind",
          container_type,
          gab_snative(gab, "bind", gab_lib_bind),
      },
      {
          "listen",
          container_type,
          gab_snative(gab, "listen", gab_lib_listen),
      },
      {
          "accept",
          container_type,
          gab_snative(gab, "accept", gab_lib_accept),
      },
      {
          "recv",
          container_type,
          gab_snative(gab, "receive", gab_lib_receive),
      },
      {
          "send",
          container_type,
          gab_snative(gab, "send", gab_lib_send),
      },
      {
          "connect",
          container_type,
          gab_snative(gab, "connect", gab_lib_connect),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

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
