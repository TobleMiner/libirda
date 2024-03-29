#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "irlap_defs.h"
#include "irlap.h"

#include "../util/eventqueue.h"

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
#include "irlap_unitdata.h"
#include "irlap_connect.h"
#include "irlap_frame_wrapper.h"
#include "irlap_test.h"
#include "../irphy/irphy.h"
#include "../util/list.h"

struct irlap {
  void* priv;
  struct irphy* phy;

  irlap_addr_t address;

  irlap_station_mode_t state;
  irlap_station_role_t role;
  bool media_busy;
  int media_busy_timer;
  size_t media_busy_counter;

  unsigned int additional_bof;

  irlap_connection_list_t connections;
  void* connection_lock;

  struct irlap_discovery discovery;

  void* phy_lock;
  void* state_lock;

  irlap_wrapper_state_t wrapper_state;

  struct eventqueue events;

  struct irlap_unitdata unitdata;

  struct irlap_connect connect;

  struct {
    struct irlap_service_discovery discovery;
    struct irlap_service_new_address new_address;
    struct irlap_service_unitdata unitdata;
    struct irlap_service_disconnect disconnect;
    struct irlap_service_connect connect;
    struct irlap_service_test test;
  } services;
};

typedef int (*irlap_frame_handler_f)(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool pf);

struct irlap_frame_handler {
  uint8_t control;
  irlap_frame_handler_f handle_cmd;
  irlap_frame_handler_f handle_resp;
};

typedef void (*irlap_indirection_f)(struct irlap* lap, void* data);

struct irlap_data_fragment {
  uint8_t* data;
  size_t len;
};

int irlap_init(struct irlap* lap, struct irphy* phy, void* priv);
void irlap_event_loop(struct irlap* lap);
int irlap_indirect_call(struct irlap* lap, int type, void* data);
int irlap_regenerate_address(struct irlap* lap);
bool irlap_is_media_busy(struct irlap* lap);
int irlap_lock_alloc(struct irlap* lap, void** lock);
void irlap_lock_free(struct irlap* lap, void* lock);
void irlap_lock_take(struct irlap* lap, void* lock);
void irlap_lock_put(struct irlap* lap, void* lock);
int irlap_lock_alloc_reentrant(struct irlap* lap, void** lock);
void irlap_lock_free_reentrant(struct irlap* lap, void* lock);
void irlap_lock_take_reentrant(struct irlap* lap, void* lock);
void irlap_lock_put_reentrant(struct irlap* lap, void* lock);
irlap_addr_t irlap_get_address(struct irlap* lap);
int irlap_set_timer(struct irlap* lap, unsigned int timeout_ms, irhal_timer_cb cb, void* priv);
int irlap_clear_timer(struct irlap* lap, int timer);
int irlap_send_frame(struct irlap* lap, irlap_frame_hdr_t* hdr, struct irlap_data_fragment* fragments, size_t num_fragments);
int irlap_send_frame_single(struct irlap* lap, irlap_frame_hdr_t* hdr, uint8_t* payload, size_t payload_len);
irphy_capability_baudrate_t irlap_get_supported_baudrates(struct irlap* lap);

#define irlap_random_u8(lap, val, min, max) (irhal_random_u8((lap)->phy->hal, (val), (min), (max)))

static inline bool irlap_frame_match_dst(struct irlap* lap, irlap_addr_t dst_address) {
  if(dst_address == IRLAP_ADDR_BCAST) {
    return true;
  }
  if(dst_address == IRLAP_ADDR_NULL) {
    return false;
  }
  return dst_address == irlap_get_address(lap);
}
