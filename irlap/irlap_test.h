#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "irlap_defs.h"

typedef void (*irlap_test_confirm_f)(irlap_addr_t src_address, uint8_t* data, size_t data_len, void* priv);

struct irlap_service_test {
  irlap_test_confirm_f confirm;
};

#define IRLAP_TEST_FRAME_LEN (sizeof(((union irlap_frame_test*)NULL)->data))

union irlap_frame_test {
  struct {
    irlap_addr_t src_address;
    irlap_addr_t dst_address;
  } __attribute__((packed));
  uint8_t data[sizeof(irlap_addr_t) + sizeof(irlap_addr_t)];
};

int irlap_test_request(struct irlap* lap, irlap_connection_addr_t conn_addr, irlap_addr_t dst_address, uint8_t* payload, size_t payload_len);
int irlap_test_handle_test_cmd(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool poll);
int irlap_test_handle_test_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool final);
