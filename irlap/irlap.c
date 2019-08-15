#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "irlap.h"
#include "irlap_frame_wrapper.h"
#include "../irhal/irhal.h"
#include "../util/crc.h"

#define LOCAL_TAG "IRDA LAP"

#define IRLAP_LOGV(lap, fmt, ...) IRHAL_LOGV(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_LOGD(lap, fmt, ...) IRHAL_LOGD(lap->phy->hal, fmt, ##__VA_ARGS__)

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

  err = irlap_lock_alloc(lap, &lap->phy_lock);
  if(err) {
    goto fail;
  }

  err = irlap_lock_alloc(lap, &lap->state_lock);
  if(err) {
    goto fail_phy_lock;
  }

fail_phy_lock:
  irlap_lock_free(lap, &lap->phy_lock);
fail:
  return err;
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
		return IRLAP_FRAME_ADDITIONAL_BOF_CONTETION;
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

  IRLAP_LOGD(lap, "==== Wrapped frame START ====");
  for(size_t i = 0; i < frame_size; i++) {
    printf("%02x ", frame_data[i]);
  }
  printf("\n");
  IRLAP_LOGD(lap, "==== Wrapped frame END ====");
	
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
