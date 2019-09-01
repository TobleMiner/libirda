#pragma once

#include <stdint.h>

#include "irlap_defs.h"
#include "../util/list.h"

typedef uint32_t irlap_baudrate_t;
typedef uint16_t irlap_turn_around_time_t;
typedef uint8_t irlap_disconnect_threshold_t;
typedef uint16_t irlap_data_size_t;

struct irlap_connect_req_qos {
  irlap_baudrate_t             baudrate;
  irlap_turn_around_time_t     max_turn_around_time;
  irlap_disconnect_threshold_t disconnect_threshold;
  irlap_data_size_t            data_size;
};

struct irlap_connect_resp_qos {
  irlap_baudrate_t             baudrate;
  irlap_data_size_t            data_size;
  irlap_disconnect_threshold_t disconnect_threshold;  
};

struct irlap_connect {
  struct irlap_connect_req_qos current_req_qos;
  irlap_addr_t current_target_addr;
  void* connect_lock;
};

int irlap_connect_init(struct irlap_connect* conn);
void irlap_connect_free(struct irlap_connect* conn);

int irlap_connect_handle_sniff_xid_req_sconn(struct irlap* lap, irlap_addr_t addr);
