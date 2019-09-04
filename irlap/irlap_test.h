#pragma once

#include <stdint.h>

#include "irlap_defs.h"

typedef void (*irlap_test_confirm_f)(irlap_addr_t src_address, uint8_t* data, size_t data_len, void* priv);

struct irlap_service_test {
  irlap_test_confirm_f confirm;
};

union irlap_frame_test {
  struct {
    irlap_addr_t src_address;
    irlap_addr_t dst_address;
  } __attribute__((packed));
  uint8_t data[sizeof(irlap_addr_t) + sizeof(irlap_addr_t)];
};

int irlap_test_request(struct irlap* lap, irlap_connection_addr_t conn_addr, irlap_addr_t dst_address, uint8_t* payload, size_t payload_len);
