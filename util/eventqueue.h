#pragma once

#include <sys/types.h>

#include "../irhal/irhal.h"

struct event {
  int type;
  void* data;
};

struct eventqueue {
  struct irhal* hal;
  void* work_lock;
  void* data_lock;
  struct event* events;
  size_t size;
  size_t num_events;
};

int eventqueue_init(struct eventqueue* queue, struct irhal* hal, size_t size);
void eventqueue_free(struct eventqueue* queue);

int eventqueue_enqueue(struct eventqueue* queue, int type, void* data);
struct event eventqueue_dequeue(struct eventqueue* queue);
