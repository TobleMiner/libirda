#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "irlap.h"
#include "irlap_frame_wrapper.h"
#include "../irhal/irhal.h"
#include "../util/crc.h"
#include "irlap_discovery.h"

#define LOCAL_TAG "IRDA LAP"

#define IRLAP_LOGV(lap, fmt, ...) IRHAL_LOGV(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_LOGD(lap, fmt, ...) IRHAL_LOGD(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_LOGI(lap, fmt, ...) IRHAL_LOGI(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_LOGW(lap, fmt, ...) IRHAL_LOGW(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_LOGE(lap, fmt, ...) IRHAL_LOGE(lap->phy->hal, fmt, ##__VA_ARGS__)

static struct irlap_frame_handler frame_handlers[] = {
  { IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_CMD_XID, irlap_discovery_handle_xid_cmd, NULL },
  { IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_RESP_XID, NULL, irlap_discovery_handle_xid_resp },
  { 0, NULL, NULL }
};

static irlap_indirection_f event_indirections[] = {
  irlap_discovery_indirect_busy,
};

static int irlap_media_busy(struct irlap* lap);
static void irlap_handle_irda_event(struct irphy* phy, irphy_event_t event, void* priv);

int irlap_init(struct irlap* lap, struct irphy* phy, struct irlap_ops* ops, void* priv) {
  int err;
  memset(lap, 0, sizeof(*lap));
  lap->phy = phy;
  lap->ops = *ops;
	lap->priv = priv;

  err = irlap_regenerate_address(lap);
  if(err) {
    goto fail;
  }

  err = irlap_lock_alloc_reentrant(lap, &lap->phy_lock);
  if(err) {
    goto fail;
  }

  err = irlap_lock_alloc_reentrant(lap, &lap->state_lock);
  if(err) {
    goto fail_phy_lock;
  }

  err = irlap_discovery_init(&lap->discovery);
  if(err) {
    goto fail_state_lock;
  }

  err = eventqueue_init(&lap->events, lap->phy->hal, 32);
  if(err) {
    goto fail_discovery;
  }

  err = irlap_media_busy(lap);
  if(err) {
    goto fail_eventqueue;
  }

  err = irphy_rx_enable(lap->phy, irlap_handle_irda_event, lap);
  if(err) {
    goto fail_eventqueue;
  }

  return 0;

fail_eventqueue:
  eventqueue_free(&lap->events);
fail_discovery:
  irlap_discovery_free(&lap->discovery);
fail_state_lock:
  irlap_lock_free_reentrant(lap, &lap->state_lock);
fail_phy_lock:
  irlap_lock_free_reentrant(lap, &lap->phy_lock);
fail:
  return err;
}

void irlap_event_loop(struct irlap* lap) {
  while(true) {
    struct event event = eventqueue_dequeue(&lap->events);
    if(event.type >= 0 && event.type < ARRAY_LEN(event_indirections)) {
      event_indirections[event.type](lap, event.data);
    } else {
      IRLAP_LOGE(lap, "BUG: event type (%d) outside indirection table bounds", event.type);
    }
  }
}

int irlap_indirect_call(struct irlap* lap, int type, void* data) {
  return eventqueue_enqueue(&lap->events, type, data);
}

bool irlap_is_media_busy(struct irlap* lap) {
	return lap->media_busy;
}

int irlap_regenerate_address(struct irlap* lap) {
  int err;
  irlap_addr_t addr;
  err = irhal_random_bytes(lap->phy->hal, &addr, sizeof(addr));
  if(err) {
    return err;
  }
  lap->address = addr;
  return 0;
}

static unsigned int irlap_get_num_extra_bof(struct irlap* lap, irlap_frame_hdr_t* hdr) {
	if(IRLAP_STATE_IS_CONTENTION(lap->state)) {
		return IRLAP_FRAME_ADDITIONAL_BOF_CONTENTION;
	}

	return lap->additional_bof;
}

int irlap_send_frame(struct irlap* lap, irlap_frame_hdr_t* hdr, uint8_t* payload, size_t payload_len) {
	int err;
	uint8_t* frame_data;
	unsigned int additional_bof = irlap_get_num_extra_bof(lap, hdr);
	ssize_t frame_size = irlap_wrapper_get_wrapped_size(IRLAP_FRAME_WRAPPER_ASYNC, hdr, payload, payload_len, additional_bof);
	if(frame_size < 0) {
		err = frame_size;
		goto fail;
	}

	frame_data = malloc(frame_size);
	if(!frame_data) {
		err = -ENOMEM;
		goto fail;
	}

  frame_size = irlap_wrapper_wrap(IRLAP_FRAME_WRAPPER_ASYNC, frame_data, frame_size, hdr, payload, payload_len, additional_bof);
	if(frame_size < 0) {
		err = frame_size;
		goto fail_alloc;
	}

/*
  IRLAP_LOGD(lap, "==== Wrapped frame START ====");
  for(size_t i = 0; i < frame_size; i++) {
    printf("%02x ", frame_data[i]);
  }
  printf("\n");
  IRLAP_LOGD(lap, "==== Wrapped frame END ====");
*/
  err = irphy_tx_enable(lap->phy);
	if(err) {
		goto fail_alloc;
	}

  frame_size = irphy_tx(lap->phy, frame_data, frame_size);
	if(frame_size < 0) {
		err = frame_size;
		goto fail_tx;
	}

	err = irphy_tx_wait(lap->phy);
fail_tx:
	irphy_tx_disable(lap->phy);
fail_alloc:
	free(frame_data);
fail:
	return err;
}

int irlap_lock_alloc(struct irlap* lap, void** lock) {
  return irhal_lock_alloc(lap->phy->hal, lock);
}

void irlap_lock_free(struct irlap* lap, void* lock) {
  irhal_lock_alloc(lap->phy->hal, lock);
}

void irlap_lock_take(struct irlap* lap, void* lock) {
  irhal_lock_take(lap->phy->hal, lock);
}

void irlap_lock_put(struct irlap* lap, void* lock) {
  irhal_lock_put(lap->phy->hal, lock);
}

int irlap_lock_alloc_reentrant(struct irlap* lap, void** lock) {
  return irhal_lock_alloc_reentrant(lap->phy->hal, lock);
}

void irlap_lock_free_reentrant(struct irlap* lap, void* lock) {
  irhal_lock_alloc_reentrant(lap->phy->hal, lock);
}

void irlap_lock_take_reentrant(struct irlap* lap, void* lock) {
  irhal_lock_take_reentrant(lap->phy->hal, lock);
}

void irlap_lock_put_reentrant(struct irlap* lap, void* lock) {
  irhal_lock_put_reentrant(lap->phy->hal, lock);
}

irlap_addr_t irlap_get_address(struct irlap* lap) {
  return lap->address;
}

int irlap_set_timer(struct irlap* lap, unsigned int timeout_ms, irhal_timer_cb cb, void* priv) {
  time_ns_t timeout = { .sec = 0 };
  timeout.nsec = (uint32_t)timeout_ms * 1000000UL;
  time_normalize(&timeout);
  return irhal_set_timer(lap->phy->hal, &timeout, cb, priv);
}

int irlap_clear_timer(struct irlap* lap, int timer) {
  return irhal_clear_timer(lap->phy->hal, timer);
}

int irlap_handle_frame(uint8_t* data, size_t len, void* priv) {
  struct irlap_frame_handler* hndlr = frame_handlers;
  struct irlap* lap = priv;
  IRLAP_LOGD(lap, "Got unwrapped frame with %zu bytes", len);
  irlap_frame_hdr_t frame_hdr;
	if(len < sizeof(frame_hdr.data)) {
    IRLAP_LOGD(lap, "Got frame shorter than %zu bytes, missing full header", sizeof(frame_hdr.data));
		return -EINVAL;
	}
  memcpy(frame_hdr.data, data, sizeof(frame_hdr.data));
	data += sizeof(frame_hdr.data);
	len -= sizeof(frame_hdr.data);
  
  IRLAP_LOGV(lap, "Frame control: %02x", frame_hdr.control);
  while(hndlr->handle_cmd != NULL || hndlr->handle_resp != NULL) {
    if(IRLAP_FRAME_MASK_POLL_FINAL(frame_hdr.control) != IRLAP_FRAME_MASK_POLL_FINAL(hndlr->control)) {
      goto next;
    }
    if(IRLAP_FRAME_IS_COMMAND(&frame_hdr) && hndlr->handle_cmd != NULL) {
      bool poll = IRLAP_FRAME_IS_POLL_FINAL(&frame_hdr);
      if(hndlr->handle_cmd(lap, NULL, data, len, poll) == IRLAP_FRAME_HANDLED) {
        return 0;
      }
    }
    if(IRLAP_FRAME_IS_RESPONSE(&frame_hdr) && hndlr->handle_resp != NULL) {
      bool final = IRLAP_FRAME_IS_POLL_FINAL(&frame_hdr);
      if(hndlr->handle_resp(lap, NULL, data, len, final) == IRLAP_FRAME_HANDLED) {
        return 0;
      }
    }
next:
    hndlr++;
  }
  return 0;
}

void irlap_media_busy_timeout(void* arg) {
  struct irlap* lap = arg;
  irlap_lock_take_reentrant(lap, lap->state_lock);
  lap->media_busy_timer = 0;
  lap->media_busy = false;
  irlap_lock_put_reentrant(lap, lap->state_lock);
}

static int irlap_media_busy(struct irlap* lap) {
  int err;
  irlap_lock_take_reentrant(lap, lap->state_lock);
  lap->media_busy = true;
  if(lap->media_busy_timer) {
    irlap_clear_timer(lap, lap->media_busy_timer);
    lap->media_busy_timer = 0;
  }
  err = irlap_set_timer(lap, IRLAP_MEDIA_BUSY_TIMEOUT, irlap_media_busy_timeout, lap);
  if(err < 0) {
    IRLAP_LOGW(lap, "Failed to start media busy timer, clearing busy flag");
    lap->media_busy = false;
  } else {
    lap->media_busy_timer = err;
  }
  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err < 0 ? err : 0;
}

static void irlap_handle_irda_event(struct irphy* phy, irphy_event_t event, void* priv) {
  struct irlap* lap = priv;
  int err = 0;
  uint8_t buff[128];
  ssize_t read_len;
  switch(event) {
    case IRPHY_EVENT_DATA_RX:
      while((read_len = irphy_rx(lap->phy, buff, sizeof(buff))) > 0) {
        if(irlap_wrapper_unwrap(IRLAP_FRAME_WRAPPER_ASYNC, &lap->wrapper_state, buff, read_len, irlap_handle_frame, lap)) {
          err = 1;
        }
      }
      if(read_len < 0) {
        IRLAP_LOGE(lap, "Failed to read from infrared phy: %zd", read_len);
      }
      if(err) {
        irlap_media_busy(lap);
      }
      break;
    case IRPHY_EVENT_FRAMING_ERROR:
    case IRPHY_EVENT_RX_OVERFLOW:
      irlap_media_busy(lap);
  }
}
