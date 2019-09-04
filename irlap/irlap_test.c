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
