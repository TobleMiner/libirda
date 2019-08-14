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
  struct irphy* phy;
  struct irlap_ops ops;

  irlap_addr_t address;
  irlap_version_t version;

  irlap_station_mode_t state;
  irlap_station_role_t role;

  irlap_connection_list_t connections;
};

union irlap_frame_hdr {
  struct {
    irlap_connection_addr_t connection_address;
    irlap_control_t control;
  };
  uint8_t data[2];
};

typedef union irlap_frame_hdr irlap_frame_hdr_t;

#define IRLAP_FRAME_IS_UNNUMBERED(hdr) (hdr.)

int irlap_init(struct irlap* lap, struct irphy* phy, struct irlap_ops* ops);
int irlap_regenerate_address(struct irlap* lap);
int irlap_send_xir(struct irlap* lap);
