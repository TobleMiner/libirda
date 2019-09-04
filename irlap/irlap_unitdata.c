#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../util/util.h"
#include "irlap.h"
#include "irlap_unitdata.h"

#define IRLAP_UNITDATA_TO_IRLAP(udata) (container_of((udata), struct irlap, unitdata))

#define LOCAL_TAG "IRDA LAP UNITDATA"

#define IRLAP_UDATA_LOGV(disc, fmt, ...) IRHAL_LOGV(IRLAP_UNITDATA_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_UDATA_LOGD(disc, fmt, ...) IRHAL_LOGD(IRLAP_UNITDATA_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_UDATA_LOGI(disc, fmt, ...) IRHAL_LOGI(IRLAP_UNITDATA_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_UDATA_LOGW(disc, fmt, ...) IRHAL_LOGW(IRLAP_UNITDATA_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_UDATA_LOGE(disc, fmt, ...) IRHAL_LOGE(IRLAP_UNITDATA_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)

int irlap_unitdata_init(struct irlap_unitdata* udata) {
  struct irlap* lap = IRLAP_UNITDATA_TO_IRLAP(udata);
  int err;

  memset(udata, 0, sizeof(*udata));
  err = irlap_lock_alloc(lap, &udata->ui_timer_lock);
  if(err) {
    IRLAP_UDATA_LOGE(udata, "Failed to allocate unitdata timer lock");
    err = -ENOMEM;
    goto fail;
  }

fail:
  return err;
}

void irlap_unitdata_free(struct irlap_unitdata* udata) {
  struct irlap* lap = IRLAP_UNITDATA_TO_IRLAP(udata);
  irlap_lock_free(lap, udata->ui_timer_lock);
}

static void irlap_udata_interval_timeout(void* priv) {
  struct irlap_unitdata* udata = priv;
  struct irlap* lap = IRLAP_UNITDATA_TO_IRLAP(udata);
  irlap_lock_take(lap, udata->ui_timer_lock);
  udata->ui_timer = 0;
  irlap_lock_put(lap, udata->ui_timer_lock);
}

int irlap_unitdata_request(struct irlap_unitdata* udata, uint8_t* data, size_t len) {
  struct irlap* lap = IRLAP_UNITDATA_TO_IRLAP(udata);
  int err = 0;
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_CMD_UI | IRLAP_CMD_POLL,
  };

  if(len > IRLAP_UNITDATA_MAX_LEN) {
    IRLAP_UDATA_LOGW(udata, "Overly long Unitdata request with %zu bytes", len);
    err = -IRLAP_ERR_UNITDATA_TOO_LONG;
    goto fail;
  }

  irlap_lock_take_reentrant(lap, lap->state_lock);
  if(lap->state != IRLAP_STATION_MODE_NDM) {
    IRLAP_UDATA_LOGD(udata, "Station not in NDM state, can't send unitdata");
    err = -IRLAP_ERR_STATION_STATE;
    goto fail_state_locked;
  }

  if(irlap_is_media_busy(lap)) {
    IRLAP_UDATA_LOGW(udata, "Media is busy, can't send unitdata");
    err = -IRLAP_ERR_MEDIA_BUSY;
    goto fail_state_locked;
  }

  irlap_lock_take(lap, udata->ui_timer_lock);
  if(!IRLAP_UNITDATA_CAN_SEND_FRAME(udata)) {
    IRLAP_UDATA_LOGD(udata, "Station not in NDM state, can't send unitdata");
    err = -IRLAP_ERR_UNITDATA_TIME_LIMIT;
    goto fail_timer_locked;
  }

  err = irlap_send_frame_single(lap, &hdr, data, len);
  if(err) {
    IRLAP_UDATA_LOGW(udata, "Failed to send unitdata frame: %d", err);
    goto fail_timer_locked;
  }

  err = irlap_set_timer(lap, IRLAP_UNITDATA_INTERVAL_MS, irlap_udata_interval_timeout, udata);
  if(err < 0) {
    IRLAP_UDATA_LOGW(udata, "Failed to set unitdata interval timer: %d", err);
    goto fail_timer_locked;
  }
  udata->ui_timer = err;
  err = 0;

fail_timer_locked:
  irlap_lock_put(lap, udata->ui_timer_lock);
fail_state_locked:
  irlap_lock_put_reentrant(lap, lap->state_lock);
fail:
  return err;
}

int irlap_unitdata_handle_ui_cmd(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool poll) {
  struct irlap_unitdata* udata = &lap->unitdata;
  IRLAP_UDATA_LOGD(udata, "Got unitdata cmd");
  if(conn) {
    IRLAP_UDATA_LOGW(udata, "Unitdata for connections not implemented yet");
    return -IRLAP_ERR_NOT_IMPLEMENTED;
  }

  if(udata->ops.indication) {
    udata->ops.indication(data, len, lap->priv);
  }

  return IRLAP_FRAME_HANDLED;
}
