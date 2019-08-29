#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "irlap.h"
#include "irlap_defs.h"

union irlap_frame_hdr {
  struct {
    irlap_connection_addr_t connection_address;
    irlap_control_t control;
  };
  uint8_t data[2];
};

typedef union irlap_frame_hdr irlap_frame_hdr_t;

struct irlap;

#include "irlap_discovery.h"
#include "irlap_frame_wrapper.h"
#include "../irphy/irphy.h"
#include "../util/list.h"

struct irlap_ops {
  struct irlap_discovery_ops discovery;
};

typedef struct list_head irlap_connection_list_t;

struct irlap {
	void* priv;
  struct irphy* phy;
  struct irlap_ops ops;

  irlap_addr_t address;

  irlap_station_mode_t state;
  irlap_station_role_t role;
	bool media_busy;
  int media_busy_timer;

	unsigned int additional_bof;

  irlap_connection_list_t connections;

  struct irlap_discovery discovery;

  void* phy_lock;
  void* state_lock;

  irlap_wrapper_state_t wrapper_state;
};

typedef int (*irlap_frame_handler_f)(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool pf);

struct irlap_frame_handler {
  uint8_t control;
  irlap_frame_handler_f handle_cmd;
  irlap_frame_handler_f handle_resp;
};

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
