#include "gab.h"
#include <threads.h>

struct channel {
  _Atomic int rc;
  v_a_char queue;
  mtx_t mutex;
};

void channel_destroy(size_t len, char data[static len]) {
  struct channel *c = *(struct channel **)data;
  c->rc--;

  if (c->rc == 0) {
    v_a_char_destroy(&c->queue);
    mtx_destroy(&c->mutex);
    free(c);
  }
};

void channel_iref(gab_value channel) {
  assert(gab_valkind(channel) == kGAB_BOX);
  struct channel *c = *(struct channel **)gab_boxdata(channel);
  c->rc++;
}

void channel_send(struct gab_triple gab, gab_value self, size_t len,
                  gab_value values[static len]) {
  struct channel *c = *(struct channel **)gab_boxdata(self);

  mtx_lock(&c->mutex);
  for (int i = 0; i < len; i++) {
    const char *msg = gab_valintocs(gab, values[i]);

    v_a_char_push(&c->queue, a_char_create(msg, strlen(msg)));
  }
  mtx_unlock(&c->mutex);
}

gab_value channel_recv(struct gab_triple gab, gab_value self) {
  struct channel *c = *(struct channel **)gab_boxdata(self);

  mtx_lock(&c->mutex);

  if (c->queue.len < 1) {
    mtx_unlock(&c->mutex);

    return gab_undefined;
  }

  a_char *ref = v_a_char_pop(&c->queue);

  mtx_unlock(&c->mutex);

  gab_value v = gab_nstring(gab, ref->len, (char *)ref->data);

  free(ref);

  return v;
}

gab_value channel_create(struct gab_triple gab) {
  struct channel *c = malloc(sizeof(struct channel));
  memset(c, 0, sizeof(struct channel));
  mtx_init(&c->mutex, mtx_plain);

  // These don't hold references to gab_values
  // This is because they are for inter-thread communication,
  // So values are serialized by one engine and parsed by another
  return gab_box(gab, (struct gab_box_argt){
                          .data = &c,
                          .size = sizeof(struct channel*),
                          .type = gab_string(gab, "gab.channel"),
                          .destructor = channel_destroy,
                      });
};
