#pragma once

#include <stdint.h>

#define IRLAP_UNITDATA_MAX_LEN     384
#define IRLAP_UNITDATA_INTERVAL_MS 500

#define IRLAP_UNITDATA_CAN_SEND_FRAME(udata) ((udata)->ui_timer == 0)

typedef void (*irlap_unitdata_indication_f)(uint8_t* data, size_t len, void* priv);

struct irlap_unitdata_ops {
  irlap_unitdata_indication_f indication;
};

struct irlap_unitdata {
  int ui_timer;
  struct irlap_unitdata_ops ops;
  void* ui_timer_lock;
};

int irlap_unitdata_init(struct irlap_unitdata* udata);
void irlap_unitdata_free(struct irlap_unitdata* udata);
int irlap_unitdata_request(struct irlap_unitdata* udata, uint8_t* data, size_t len);
int irlap_unitdata_handle_ui_cmd(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool poll);
