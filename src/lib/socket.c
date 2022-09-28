#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

gab_value gab_lib_sock(gab_engine *eng, gab_value *argv, u8 argc) {

  i32 result = socket(AF_INET, SOCK_STREAM, 0);

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_NUMBER(result);
}

gab_value gab_lib_bind(gab_engine *eng, gab_value *argv, u8 argc) {

  if (!GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_NUMBER(argv[1])) {
    return GAB_VAL_NULL();
  }

  i32 port = htons(GAB_VAL_TO_NUMBER(argv[1]));

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr = {.s_addr = INADDR_ANY},
                             .sin_port = port};

  i32 result =
      bind(GAB_VAL_TO_NUMBER(argv[0]), (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_BOOLEAN(true);
}

gab_value gab_lib_listen(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 2 || !GAB_VAL_IS_NUMBER(argv[0]) || !GAB_VAL_IS_NUMBER(argv[1])) {
    return GAB_VAL_NULL();
  }

  i32 result = listen(GAB_VAL_TO_NUMBER(argv[0]), GAB_VAL_TO_NUMBER(argv[1]));

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_IS_BOOLEAN(true);
}

gab_value gab_lib_accept(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 1 || !GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  i32 socket = GAB_VAL_TO_NUMBER(argv[0]);

  i32 result = accept(socket, &addr, &addrlen);

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_NUMBER(result);
}

gab_value gab_lib_connect(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 3 || !GAB_VAL_IS_NUMBER(argv[0]) || !GAB_VAL_TO_NUMBER(argv[2]) ||
      !GAB_VAL_IS_STRING(argv[1])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *ip = GAB_VAL_TO_STRING(argv[1]);

  i32 port = htons(GAB_VAL_TO_NUMBER(argv[2]));

  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = port};

  if (inet_pton(AF_INET, (char *)ip->data, &addr.sin_addr) <= 0) {
    return GAB_VAL_NULL();
  }

  i32 result = connect(GAB_VAL_TO_NUMBER(argv[0]), (struct sockaddr *)&addr,
                       sizeof(addr));

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_BOOLEAN(true);
}

gab_value gab_lib_receive(gab_engine *eng, gab_value *argv, u8 argc) {
  if (argc != 1 || !GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  i8 buffer[1024] = {0};

  i32 result = recv(GAB_VAL_TO_NUMBER(argv[0]), buffer, 1024, 0);

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  s_i8 msg = {.data = buffer, .len = result};

  return GAB_VAL_OBJ(gab_obj_string_create(eng, msg));
}

gab_value gab_lib_send(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  if (!GAB_VAL_IS_STRING(argv[1])) {
    return GAB_VAL_NULL();
  }

  gab_obj_string *msg = GAB_VAL_TO_STRING(argv[1]);

  i32 result = send(GAB_VAL_TO_NUMBER(argv[0]), msg->data, msg->len, 0);

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_BOOLEAN(true);
}

gab_value gab_lib_close(gab_engine *eng, gab_value *argv, u8 argc) {
  if (!GAB_VAL_IS_NUMBER(argv[0])) {
    return GAB_VAL_NULL();
  }

  i32 result = shutdown(GAB_VAL_TO_NUMBER(argv[0]), SHUT_RDWR);

  if (result < 0) {
    return GAB_VAL_NULL();
  }

  return GAB_VAL_BOOLEAN(true);
}

gab_value gab_mod(gab_engine *gab) {
  s_i8 keys[] = {
      s_i8_cstr("sock"),    s_i8_cstr("bind"),    s_i8_cstr("listen"),
      s_i8_cstr("accept"),  s_i8_cstr("receive"), s_i8_cstr("send"),
      s_i8_cstr("connect"), s_i8_cstr("close"),
  };

  gab_value values[] = {
      GAB_BUILTIN(sock, 0),
      GAB_BUILTIN(bind, 2),
      GAB_BUILTIN(listen, 2),
      GAB_BUILTIN(accept, 1),
      GAB_BUILTIN(receive, 1),
      GAB_BUILTIN(send, 2),
      GAB_BUILTIN(connect, 3),
      GAB_BUILTIN(close, 1),
  };

  return gab_bundle(gab, sizeof(values) / sizeof(gab_value), keys, values);
}
