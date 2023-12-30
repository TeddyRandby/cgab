#include "gab.h"
#include <threads.h>

struct channel {
  atomic_int rc;
  v_a_char in_queue;
  v_a_char out_queue;
  mtx_t mutex;
};

void channel_destroy(size_t len, unsigned char data[static len]) {
  struct channel *c = (void *)data;

  // If this was the last reference to the channel, we can destroy its data
  if (!c->rc--) {
    v_a_char_destroy(&c->in_queue);
    v_a_char_destroy(&c->out_queue);
    mtx_destroy(&c->mutex);
  }
};

void channel_send(struct gab_triple gab, gab_value self, size_t len,
                  gab_value values[static len]) {
  struct channel *c = gab_boxdata(self);

  mtx_lock(&c->mutex);
  for (int i = 1; i < len; i++) {
    gab_value msg = gab_valintos(gab, values[i]);

    const char *ref = gab_valintocs(gab, msg);

    v_a_char_push(&c->in_queue, a_char_create(ref, strlen(ref)));
  }
  mtx_unlock(&c->mutex);
}

gab_value channel_recv(struct gab_triple gab, gab_value self) {
  struct channel *c = gab_boxdata(self);

  mtx_lock(&c->mutex);

  if (c->out_queue.len < 1) {
    mtx_unlock(&c->mutex);

    return gab_undefined;
  }

  a_char *ref = v_a_char_pop(&c->out_queue);

  gab_value v = gab_nstring(gab, ref->len, (char *)ref->data);

  free(ref);

  mtx_unlock(&c->mutex);

  return v;
}

gab_value channel_create(struct gab_triple gab) {
  struct channel c = {};
  mtx_init(&c.mutex, mtx_plain);

  // These don't hold references to gab_values
  // This is because they are for inter-thread communication,
  // So values are serialized by one engine and parsed by another
  return gab_box(gab, (struct gab_box_argt){
                          .type = gab_string(gab, "Channel"),
                          .destructor = channel_destroy,
                          .data = &c,
                          .size = sizeof(struct channel),
                      });
};
