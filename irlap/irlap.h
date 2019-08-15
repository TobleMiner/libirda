#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "irlap_defs.h"
#include "irlap_discovery.h"
#include "../irphy/irphy.h"
#include "../util/list.h"

struct irlap_ops {
  struct irlap_discovery_ops discovery;
};

typedef struct list_head irlap_connection_list_t;

struct irlap_connection {
  irlap_connection_addr_t connection_address;
};

struct irlap {
	void* priv;
  struct irphy* phy;
  struct irlap_ops ops;

  irlap_addr_t address;

  irlap_station_mode_t state;
  irlap_station_role_t role;
	bool media_busy;

	unsigned int additional_bof;

  irlap_connection_list_t connections;

  struct irlap_discovery discovery;

  void* phy_lock;
  void* state_lock;
};

union irlap_frame_hdr {
  struct {
    irlap_connection_addr_t connection_address;
    irlap_control_t control;
  };
  uint8_t data[2];
};

typedef union irlap_frame_hdr irlap_frame_hdr_t;

int irlap_init(struct irlap* lap, struct irphy* phy, struct irlap_ops* ops, void* priv);
int irlap_regenerate_address(struct irlap* lap);
bool irlap_is_media_busy(struct irlap* lap);
int irlap_lock_alloc(struct irlap* lap, void** lock);
void irlap_lock_free(struct irlap* lap, void* lock);
void irlap_lock_take(struct irlap* lap, void* lock);
void irlap_lock_put(struct irlap* lap, void* lock);
irlap_addr_t irlap_get_address(struct irlap* lap);
int irlap_set_timer(struct irlap* lap, unsigned int timeout_ms, irhal_timer_cb cb, void* priv);
int irlap_clear_timer(struct irlap* lap, int timer);
int irlap_send_frame(struct irlap* lap, irlap_frame_hdr_t* hdr, uint8_t* payload, size_t payload_len);
