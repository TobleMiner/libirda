#pragma once

#include <stdint.h>

#include "irlap_defs.h"

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
  int p_timer;
  struct irlap_connect_req_qos current_req_qos;
  irlap_addr_t current_target_addr;
  void* connect_lock;
};


#define IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN 22

union irlap_snrm_frame {
  struct {
    irlap_addr_t            src_address;
    irlap_addr_t            dst_address;
    irlap_connection_addr_t connection_addr;
    uint8_t                 negotiation_params[IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN];
  } __attribute__((packed));
  uint8_t data[9];
  uint8_t data_params[9 + IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN];
};

int irlap_connect_init(struct irlap_connect* conn);
void irlap_connect_free(struct irlap_connect* conn);

int irlap_connect_handle_sniff_xid_req_sconn(struct irlap* lap, irlap_addr_t addr);
