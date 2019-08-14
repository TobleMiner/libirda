#pragma once

#include "../util/list.h"
#include "irlap_defs.h"

typedef struct list_head irlap_discovery_log_list_t;

struct irlap_discovery_log {
  bool            solicited;
  bool            sniff;
  irlap_addr_t    device_address;
  irlap_version_t irlap_version;
  struct {
    char data[32];
    uint8_t len;
  }               discovery_info;
};

struct irlap_discovery_log_entry {
  irlap_discovery_log_list_t list;
  struct irlap_discovery_log discovery_log;
};


typedef int (*irlap_discovery_indication)(struct irlap_discovery_log* log);
typedef int (*irlap_discovery_confirm)(irlap_discovery_log_list_t* list);

struct irlap_discovery_ops {
  void* priv;
  irlap_discovery_indication indication;
  irlap_discovery_confirm confirm;
};
