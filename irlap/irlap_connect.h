#pragma once

#include <stdint.h>

#include "irlap_defs.h"
#include "../util/list.h"
#include "irlap_connection.h"

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

union irlap_ua_frame {
  struct {
    irlap_addr_t src_address;
    irlap_addr_t dst_address;
    uint8_t      negotiation_params[IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN];
  } __attribute__((packed));
  uint8_t data[8];
  uint8_t data_params[8 + IRLAP_NEGOTIATION_PARAMETERS_MAX_LEN];
};

struct irlap_unacked_data {
  uint8_t _keep;
};

typedef void (*irlap_connect_indication_f)(irlap_addr_t src_address, irlap_connection_addr_t hndl, struct irlap_connect_resp_qos* resp_qos, void* priv);
typedef void (*irlap_connect_confirm_f)(irlap_connection_addr_t hndl, struct irlap_connect_resp_qos* resp_qos, void* priv);

struct irlap_service_connect {
  irlap_connect_indication_f indication;
  irlap_connect_confirm_f confirm;
};

typedef void (*irlap_disconnect_indication_f)(irlap_connection_addr_t hndl, struct irlap_unacked_data* data, void* priv);

struct irlap_service_disconnect {
  irlap_disconnect_indication_f indication;
};

int irlap_connect_init(struct irlap_connect* conn);
void irlap_connect_free(struct irlap_connect* conn);

int irlap_connect_handle_ua_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool pf);
int irlap_connect_handle_dm_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool pf);
int irlap_connect_handle_sniff_xid_req_sconn(struct irlap* lap, irlap_addr_t addr);
