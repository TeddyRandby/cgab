#include "gab.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define SOCKET_FAMILY "socket.family"
#define SOCKET_TYPE "socket.type"

#define SOCKET_BOX_TYPE "gab.os.socket"
#define CONNECTEDSOCKET_BOX_TYPE "gab.connected.socket"

void gab_container_socket_cb(size_t len, char data[static len]) {
  shutdown((int64_t)data, SHUT_RDWR);
  close((int64_t)data);
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
      return gab_pktypemismatch(gab, argv[1], kGAB_RECORD);

    gab_value domain_val = gab_srecat(gab, argv[1], SOCKET_FAMILY);
    gab_value type_val = gab_srecat(gab, argv[1], SOCKET_TYPE);

    if (gab_valkind(domain_val) != kGAB_NUMBER)
      return gab_pktypemismatch(gab, domain_val, kGAB_NUMBER);

    if (gab_valkind(type_val) != kGAB_NUMBER)
      return gab_pktypemismatch(gab, type_val, kGAB_NUMBER);

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

  int sockfd = socket(domain, type, 0);

  if (sockfd < 0) {
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, "socket failed to open"));
    return nullptr;
  }

  gab_vmpush(gab_vm(gab), gab_ok,
             gab_box(gab, (struct gab_box_argt){
                              .type = gab_string(gab, SOCKET_BOX_TYPE),
                              .destructor = gab_container_socket_cb,
                              .size = sizeof(sockfd),
                              .data = &sockfd,
                          }));

  return nullptr;
}

a_gab_value *gab_lib_bind(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  int socket = *(int *)gab_boxdata(gab_arg(0));

  int family, port;

  gab_value config = gab_arg(1);

  switch (gab_valkind(config)) {
  case kGAB_NUMBER:
    family = AF_INET;
    port = htons(gab_valton(config));
    break;

  case kGAB_RECORD: {
    gab_value family_value = gab_srecat(gab, config, SOCKET_FAMILY);

    if (gab_valkind(family_value) != kGAB_NUMBER)
      return gab_pktypemismatch(gab, family_value, kGAB_NUMBER);

    gab_value port_value = gab_srecat(gab, config, "port");

    if (gab_valkind(port_value) != kGAB_NUMBER)
      return gab_pktypemismatch(gab, port_value, kGAB_NUMBER);

    family = gab_valton(family_value);

    port = htons(gab_valton(port_value));

    break;
  }

  default:
    return gab_pktypemismatch(gab, config, kGAB_NUMBER);
  }

  int result = bind(socket,
                    (struct sockaddr *)(struct sockaddr_in[]){{
                        .sin_family = family,
                        .sin_addr = {.s_addr = INADDR_ANY},
                        .sin_port = port,
                    }},
                    sizeof(struct sockaddr_in));

  if (result < 0)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
  else
    gab_vmpush(gab_vm(gab), gab_ok);

  return nullptr;
}

a_gab_value *gab_lib_listen(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  gab_value port = gab_arg(1);

  if (gab_valkind(port) != kGAB_NUMBER)
    return gab_pktypemismatch(gab, port, kGAB_NUMBER);

  int socket = *(int *)gab_boxdata(gab_arg(0));

  int result = listen(socket, gab_valton(port));

  if (result < 0)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, "LISTEN_FAILED"));
  else
    gab_vmpush(gab_vm(gab), gab_ok);

  return nullptr;
}

a_gab_value *gab_lib_accept(struct gab_triple gab, size_t argc,
                            gab_value argv[argc]) {
  int socket = *(int *)gab_boxdata(gab_arg(0));

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  int64_t connfd = accept(socket, &addr, &addrlen);

  if (connfd < 0) {
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
    return nullptr;
  }

  gab_vmpush(gab_vm(gab), gab_ok,
             gab_box(gab, (struct gab_box_argt){
                              .type = gab_string(gab, CONNECTEDSOCKET_BOX_TYPE),
                              .destructor = gab_container_socket_cb,
                              .size = sizeof(connfd),
                              .data = &connfd,
                          }));

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

  int socket = *(int *)gab_boxdata(gab_arg(0));

  const char *ip = gab_strdata(&host);

  int cport = htons(gab_valton(port));

  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = cport};

  int result = inet_pton(AF_INET, ip, &addr.sin_addr);

  if (result <= 0) {
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, "INET_PTON_FAILED"));
    return nullptr;
  }

  result = connect(socket, (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
  else
    gab_vmpush(
        gab_vm(gab), gab_ok,
        gab_box(gab, (struct gab_box_argt){
                         .type = gab_string(gab, CONNECTEDSOCKET_BOX_TYPE),
                         .destructor = gab_container_socket_cb,
                         .size = sizeof(socket),
                         .data = &socket,
                     }));

  return nullptr;
}

a_gab_value *gab_lib_receive(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  char buffer[1024] = {0};

  int socket = *(int *)gab_boxdata(gab_arg(0));

  int32_t result = recv(socket, buffer, 1024, 0);

  if (result < 0)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(result)));
  else
    gab_vmpush(gab_vm(gab), gab_ok, gab_nstring(gab, result, buffer));

  return nullptr;
}

a_gab_value *gab_lib_send(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value msg = gab_arg(1);

  if (gab_valkind(msg) != kGAB_STRING)
    return gab_pktypemismatch(gab, msg, kGAB_STRING);

  int socket = *(int *)gab_boxdata(gab_arg(0));

  int32_t result = send(socket, gab_strdata(&msg), gab_strlen(msg), 0);

  if (result < 0)
    gab_vmpush(gab_vm(gab), gab_err, gab_string(gab, strerror(errno)));
  else
    gab_vmpush(gab_vm(gab), gab_ok);

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value container_type = gab_string(gab, "gab.socket");
  gab_value connected_container_type = gab_string(gab, "gab.connected.socket");

  gab_def(gab,
          {
              mGAB_CALL,
              gab_strtosig(container_type),
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
              "connect",
              container_type,
              gab_snative(gab, "connect", gab_lib_connect),
          },
          {
              "recv",
              connected_container_type,
              gab_snative(gab, "receive", gab_lib_receive),
          },
          {
              "send",
              connected_container_type,
              gab_snative(gab, "send", gab_lib_send),
          });

  const char *constant_names[] = {
      "family.af_inet",
      "type.stream",
  };

  gab_value constant_values[] = {
      gab_number(AF_INET),
      gab_number(SOCK_STREAM),
  };

  gab_value constants = gab_srecord(gab, LEN_CARRAY(constant_names),
                                    constant_names, constant_values);

  gab_value res[] = {container_type, connected_container_type, constants};

  return a_gab_value_create(res, 3);
}
