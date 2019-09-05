#include <errno.h>
#include <string.h>

#include "irlap_test.h"
#include "irlap.h"

#define LOCAL_TAG "IRDA LAP TEST"

#define IRLAP_TEST_LOGV(lap, fmt, ...) IRHAL_LOGV(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_TEST_LOGD(lap, fmt, ...) IRHAL_LOGD(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_TEST_LOGI(lap, fmt, ...) IRHAL_LOGI(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_TEST_LOGW(lap, fmt, ...) IRHAL_LOGW(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_TEST_LOGE(lap, fmt, ...) IRHAL_LOGE(lap->phy->hal, fmt, ##__VA_ARGS__)

int irlap_test_request(struct irlap* lap, irlap_connection_addr_t conn_addr, irlap_addr_t dst_address, uint8_t* payload, size_t payload_len) {
  int err;
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(conn_addr),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_CMD_TEST | IRLAP_CMD_POLL,
  };
  union irlap_frame_test frame = {
    .dst_address = dst_address,
  };
  struct irlap_data_fragment fragments[] = {
    { frame.data, sizeof(frame.data) },
    { payload, payload_len },
  };

  irlap_lock_take_reentrant(lap, lap->state_lock);
  frame.src_address = irlap_get_address(lap);
  switch(lap->state) {
    case IRLAP_STATION_MODE_NDM:
      if(irlap_is_media_busy(lap)) {
        IRLAP_TEST_LOGD(lap, "Media busy, can't send test cmd");
        err = -IRLAP_ERR_MEDIA_BUSY;
        goto fail_state_locked;
      }
      hdr.connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST);
      break;
    default:
      IRLAP_TEST_LOGD(lap, "Not in NDM state, can't send test cmd");
      err = -IRLAP_ERR_STATION_STATE;
      goto fail_state_locked;      
  }

  err = irlap_send_frame(lap, &hdr, fragments, ARRAY_LEN(fragments));
  if(err) {
      IRLAP_TEST_LOGE(lap, "Failed to send test cmd to %08x: %d", dst_address, err);
  }

fail_state_locked:
  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

static int handle_test_cmd_ndm(struct irlap* lap, union irlap_frame_test* frame, uint8_t* payload, size_t payload_len) {
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_RESP_TEST | IRLAP_RESP_FINAL,
  };  
  union irlap_frame_test resp_frame = {
    .src_address = irlap_get_address(lap),
    .dst_address = frame->src_address,
  };
  struct irlap_data_fragment fragments[] = {
    { resp_frame.data, sizeof(resp_frame.data) },
    { payload, payload_len },
  };
  return irlap_send_frame(lap, &hdr, fragments, ARRAY_LEN(fragments));
}

static int verify_test_frame(struct irlap* lap, union irlap_frame_test* frame, uint8_t** data, size_t* len) {
  if(*len < IRLAP_TEST_FRAME_LEN) {
    IRLAP_TEST_LOGW(lap, "Test frame too short, %zu bytes < %zu bytes", len, IRLAP_TEST_FRAME_LEN);
    return -EINVAL;
  }

  memcpy(frame->data, *data, IRLAP_TEST_FRAME_LEN);
  *data += IRLAP_TEST_FRAME_LEN;
  *len -= IRLAP_TEST_FRAME_LEN;

  if(!irlap_frame_match_dst(lap, frame->dst_address)) {
    IRLAP_TEST_LOGD(lap, "Ignoring test frame not intended for us, dst: %08x", frame->dst_address);
    return IRLAP_FRAME_NOT_HANDLED;
  }

  return 0;
}

int irlap_test_handle_test_cmd(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool poll) {
  int err;
  union irlap_frame_test frame;

  if(!poll) {
    IRLAP_TEST_LOGW(lap, "Got invalid test cmd without poll bit set");
    return -IRLAP_ERR_POLL;
  }

  err = verify_test_frame(lap, &frame, &data, &len);
  if(err) {
    return err;
  }

  irlap_lock_take_reentrant(lap, lap->state_lock);
  switch(lap->state) {
    case IRLAP_STATION_MODE_NDM:
      err = handle_test_cmd_ndm(lap, &frame, data, len);
      break;
    default:
      IRLAP_TEST_LOGD(lap, "Not in NDM state, can't respond to test cmd");
      err = -IRLAP_ERR_STATION_STATE;
  }
  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

static int handle_test_resp_ndm(struct irlap* lap, union irlap_frame_test* frame, uint8_t* data, size_t len) {
  if(lap->services.test.confirm) {
    lap->services.test.confirm(frame->src_address, data, len, lap->priv);
  }
  return IRLAP_FRAME_HANDLED;
}

int irlap_test_handle_test_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool final) {
  int err;
  union irlap_frame_test frame;

  if(!final) {
    IRLAP_TEST_LOGW(lap, "Got invalid test resp without final bit set");
    return -IRLAP_ERR_POLL;
  }

  err = verify_test_frame(lap, &frame, &data, &len);
  if(err) {
    return err;
  }

  irlap_lock_take_reentrant(lap, lap->state_lock);
  switch(lap->state) {
    case IRLAP_STATION_MODE_NDM:
      err = handle_test_resp_ndm(lap, &frame, data, len);
      break;
    default:
      IRLAP_TEST_LOGD(lap, "Not in NDM state, can't respond to test cmd");
      err = -IRLAP_ERR_STATION_STATE;
  }
  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}
