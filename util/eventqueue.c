#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "eventqueue.h"

int eventqueue_init(struct eventqueue* queue, struct irhal* hal, size_t size) {
  int err;

  memset(queue, 0, sizeof(*queue));
  queue->hal = hal;

  queue->events = calloc(size, sizeof(struct event));
  if(!queue->events) {
    err = -ENOMEM;
    goto fail;
  }

  queue->size = size;

  err = irhal_lock_alloc(hal, &queue->work_lock);
  if(err) {
    goto fail_queue_alloc;
  }

  err = irhal_lock_alloc(hal, &queue->data_lock);
  if(err) {
    goto fail_work_lock_alloc;
  }

  return 0;

fail_work_lock_alloc:
  irhal_lock_free(hal, queue->work_lock);
fail_queue_alloc:
  free(queue->events);
fail:
  return err;
}

void eventqueue_free(struct eventqueue* queue) {
  irhal_lock_free(queue->hal, queue->data_lock);
  irhal_lock_free(queue->hal, queue->work_lock);
  free(queue->events);
}

int eventqueue_enqueue(struct eventqueue* queue, int type, void* data) {
  int err = 0;
  irhal_lock_take(queue->hal, queue->data_lock);

  if(queue->num_events >= queue->size) {
    err = -ENOBUFS;
    goto fail;
  }

  queue->events[queue->num_events].type = type;
  queue->events[queue->num_events++].data = data;

  irhal_lock_put(queue->hal, queue->work_lock);

fail:
  irhal_lock_put(queue->hal, queue->data_lock);
  return err;
}

struct event eventqueue_dequeue(struct eventqueue* queue) {
  struct event event;
retry:
  irhal_lock_take(queue->hal, queue->work_lock);
  irhal_lock_take(queue->hal, queue->data_lock);

  if(queue->num_events == 0) {
    irhal_lock_put(queue->hal, queue->data_lock);
    goto retry;
  }
  event = queue->events[--queue->num_events];

  irhal_lock_put(queue->hal, queue->data_lock);
  return event;
}
