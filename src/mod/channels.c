#include "gab.h"
#include <threads.h>

#define T a_char *
#define NAME a_char
#define DEF_T nullptr
#include "queue.h"

struct channel {
  _Atomic int rc;
  q_a_char queue;
  mtx_t mutex;
};

void channel_iref(gab_value channel) {
  assert(gab_valkind(channel) == kGAB_BOX);
  struct channel *c = *(struct channel **)gab_boxdata(channel);
  c->rc++;
}

void channel_copy(size_t len, char data[static len]) {
  struct channel *c = *(struct channel **)data;
  c->rc++;
}

void channel_destroy(size_t len, char data[static len]) {
  struct channel *c = *(struct channel **)data;
  c->rc--;

  if (c->rc == 0) {
    mtx_destroy(&c->mutex);
    free(c);
  }
};

bool channel_isempty(gab_value self) {
  struct channel *c = *(struct channel **)gab_boxdata(self);

  return q_a_char_is_empty(&c->queue);
}

void channel_send(struct gab_triple gab, gab_value self, size_t len,
                  gab_value values[static len]) {
  struct channel *c = *(struct channel **)gab_boxdata(self);

  mtx_lock(&c->mutex);
  for (int i = 0; i < len; i++) {
    const char *msg = gab_valintocs(gab, values[i]);

    q_a_char_push(&c->queue, a_char_create(msg, strlen(msg)));
  }
  mtx_unlock(&c->mutex);
}

gab_value channel_recv(struct gab_triple gab, gab_value self) {
  struct channel *c = *(struct channel **)gab_boxdata(self);

  mtx_lock(&c->mutex);

  if (q_a_char_is_empty(&c->queue)) {
    mtx_unlock(&c->mutex);

    return gab_undefined;
  }

  a_char *ref = q_a_char_pop(&c->queue);

  mtx_unlock(&c->mutex);

  if (!ref)
    return gab_undefined;

  gab_value v = gab_nstring(gab, ref->len, (char *)ref->data);

  free(ref);

  return v;
}

gab_value channel_create(struct gab_triple gab) {
  struct channel *c = malloc(sizeof(struct channel));
  memset(c, 0, sizeof(struct channel));
  mtx_init(&c->mutex, mtx_plain);
  q_a_char_create(&c->queue);

  // These don't hold references to gab_values
  // This is because they are for inter-thread communication,
  // So values are serialized by one engine and parsed by another
  return gab_box(gab, (struct gab_box_argt){
                          .data = &c,
                          .size = sizeof(struct channel *),
                          .type = gab_string(gab, "gab.channel"),
                          .destructor = channel_destroy,
                          .copier = channel_copy,
                      });
};

a_gab_value *gab_lib_channel(struct gab_triple gab, size_t argc,
                             gab_value argv[argc]) {
  gab_value c = channel_create(gab);
  channel_iref(c);
  gab_vmpush(gab_vm(gab), c);
  return nullptr;
}

a_gab_value *gab_lib_send(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  channel_send(gab, argv[0], argc - 1, argv + 1);
  return nullptr;
}

a_gab_value *gab_lib_empty(struct gab_triple gab, size_t argc, gab_value argv[argc]) {
  bool res = channel_isempty(gab_arg(0));
  gab_vmpush(gab_vm(gab), gab_bool(res));
  return nullptr;
}

a_gab_value *gab_lib_recv(struct gab_triple gab, size_t argc,
                          gab_value argv[argc]) {
  gab_value v = channel_recv(gab, argv[0]);
  if (v == gab_undefined) {
    gab_vmpush(gab_vm(gab), gab_none);
  } else {
    gab_vmpush(gab_vm(gab), gab_ok, v);
  }

  return nullptr;
}

a_gab_value *gab_lib(struct gab_triple gab) {
  gab_value channel_t = gab_string(gab, "gab.channel");

  struct gab_spec_argt specs[] = {
      {
          mGAB_CALL,
          gab_strtosig(channel_t),
          gab_snative(gab, "gab.channel", gab_lib_channel),
      },
      {
          "empty?",
          channel_t,
          gab_snative(gab, "empty?", gab_lib_empty),
      },
      {
          "<-",
          channel_t,
          gab_snative(gab, "send", gab_lib_send),
      },
      {
          "->",
          channel_t,
          gab_snative(gab, "receive", gab_lib_recv),
      },
  };

  gab_nspec(gab, sizeof(specs) / sizeof(specs[0]), specs);

  return nullptr;
}
