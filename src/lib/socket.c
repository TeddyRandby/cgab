#include "include/core.h"
#include "include/gab.h"
#include "include/object.h"
#include "include/value.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>

void gab_container_socket_cb(void *data) { shutdown((i64)data, SHUT_RDWR); }

void gab_lib_sock(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {

  i64 result = socket(AF_INET, SOCK_STREAM, 0);

  if (result < 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not open socket"),
    };

    gab_push(vm, 2, res);

    return;
  }

  gab_value res[2] = {
      GAB_STRING("ok"),
      GAB_CONTAINER(GAB_STRING("Socket"), gab_container_socket_cb, NULL,
                    (void *)result),
  };

  gab_push(vm, 2, res);

  gab_val_dref(vm, res[1]);

  return;
}

void gab_lib_bind(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {

  if (argc != 2 || !GAB_VAL_IS_CONTAINER(argv[0]) ||
      !GAB_VAL_IS_NUMBER(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_bind");
    return;
  }

  i64 socket = (i64)GAB_VAL_TO_CONTAINER(argv[0])->data;

  i32 port = htons(GAB_VAL_TO_NUMBER(argv[1]));

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr = {.s_addr = INADDR_ANY},
                             .sin_port = port};

  i32 result = bind(socket, (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not bind socket"),
    };

    gab_push(vm, 2, res);

    return;
  }

  gab_value res = GAB_STRING("ok");

  gab_push(vm, 1, &res);
}

void gab_lib_listen(gab_engine *gab, gab_vm *vm, u8 argc,
                    gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_CONTAINER(argv[0]) ||
      !GAB_VAL_IS_NUMBER(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_listen");
    return;
  }

  i64 socket = (i64)GAB_VAL_TO_CONTAINER(argv[0])->data;

  i32 result = listen(socket, GAB_VAL_TO_NUMBER(argv[1]));

  if (result < 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not listen"),
    };

    gab_push(vm, 2, res);

    return;
  }

  gab_value res = GAB_STRING("ok");

  gab_push(vm, 1, &res);
}

void gab_lib_accept(gab_engine *gab, gab_vm *vm, u8 argc,
                    gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_accept");
    return;
  }

  i64 socket = (i64)GAB_VAL_TO_CONTAINER(argv[0])->data;

  struct sockaddr addr = {0};
  socklen_t addrlen = 0;

  i64 result = accept(socket, &addr, &addrlen);

  if (result < 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not accept connection"),
    };

    gab_push(vm, 2, res);

    return;
  }

  gab_value res[2] = {
      GAB_STRING("ok"),
      GAB_CONTAINER(GAB_STRING("socket"), gab_container_socket_cb, NULL,
                    (void *)result),
  };

  gab_push(vm, 2, res);

  gab_val_dref(vm, res[1]);
}

void gab_lib_connect(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
  if (argc != 3 || !GAB_VAL_TO_NUMBER(argv[2]) || !GAB_VAL_IS_STRING(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_connect");
    return;
  }

  i64 socket = (i64)GAB_VAL_TO_CONTAINER(argv[0])->data;

  gab_obj_string *ip = GAB_VAL_TO_STRING(argv[1]);

  i32 port = htons(GAB_VAL_TO_NUMBER(argv[2]));

  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = port};

  if (inet_pton(AF_INET, (char *)ip->data, &addr.sin_addr) <= 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not convert ip to binary"),
    };

    gab_push(vm, 2, res);

    return;
  }

  i32 result = connect(socket, (struct sockaddr *)&addr, sizeof(addr));

  if (result < 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not connect"),
    };

    gab_push(vm, 2, res);

    return;
  }

  gab_value res = GAB_STRING("ok");

  gab_push(vm, 1, &res);
}

void gab_lib_receive(gab_engine *gab, gab_vm *vm, u8 argc,
                     gab_value argv[argc]) {
  if (argc != 1) {
    gab_panic(gab, vm, "Invalid call to gab_lib_receive");
    return;
  }

  i8 buffer[1024] = {0};

  i64 socket = (i64)GAB_VAL_TO_CONTAINER(argv[0])->data;

  i32 result = recv(socket, buffer, 1024, 0);

  if (result < 0) {
    gab_value res[2] = {
        GAB_STRING("err"),
        GAB_STRING("Could not receive"),
    };

    gab_push(vm, 2, res);

    return;
  }

  s_i8 msg = {.data = buffer, .len = result};

  gab_value res[2] = { 
      GAB_STRING("ok"),
      GAB_VAL_OBJ(gab_obj_string_create(gab, msg)),
  };

  gab_push(vm, 2, res);
}

void gab_lib_send(gab_engine *gab, gab_vm *vm, u8 argc, gab_value argv[argc]) {
  if (argc != 2 || !GAB_VAL_IS_STRING(argv[1])) {
    gab_panic(gab, vm, "Invalid call to gab_lib_receive");
    return;
  }

  i64 socket = (i64)GAB_VAL_TO_CONTAINER(argv[0])->data;

  gab_obj_string *msg = GAB_VAL_TO_STRING(argv[1]);

  i32 result = send(socket, msg->data, msg->len, 0);

  if (result < 0) {
    gab_value res[2] = { 
        GAB_STRING("err"),
        GAB_STRING("Could not send"),
    };

    gab_push(vm, 2, res);

    return;
  }

  gab_value res = GAB_STRING("ok");

  gab_push(vm, 1, &res);
}

gab_value gab_mod(gab_engine *gab, gab_vm *vm) {
  gab_value names[] = {
      GAB_STRING("socket"),  GAB_STRING("bind"),    GAB_STRING("listen"),
      GAB_STRING("accept"),  GAB_STRING("receive"), GAB_STRING("send"),
      GAB_STRING("connect"),
  };

  gab_value container_type = GAB_STRING("Socket");

  gab_value types[] = {
      GAB_VAL_NIL(),  container_type, container_type, container_type,
      container_type, container_type, container_type,
  };

  gab_value values[] = {
      GAB_BUILTIN(sock),    GAB_BUILTIN(bind),    GAB_BUILTIN(listen),
      GAB_BUILTIN(accept),  GAB_BUILTIN(receive), GAB_BUILTIN(send),
      GAB_BUILTIN(connect),
  };

  assert(LEN_CARRAY(names) == LEN_CARRAY(types));
  assert(LEN_CARRAY(values) == LEN_CARRAY(types));
  assert(LEN_CARRAY(values) == LEN_CARRAY(names));

  for (u64 i = 0; i < LEN_CARRAY(names); i++) {
    gab_specialize(gab, vm, names[i], types[i], values[i]);
    gab_val_dref(vm, values[i]);
  }

  return GAB_VAL_NIL();
}
