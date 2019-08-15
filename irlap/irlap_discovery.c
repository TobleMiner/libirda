#include <errno.h>
#include <string.h>

#include "irlap_discovery.h"
#include "irlap.h"
#include "../util/util.h"

static int8_t irlap_discovery_slot_table[17] = { -1, 0b00, -1, -1, -1, -1, 0b01, -1, 0b10, -1, -1, -1, -1, -1, -1, -1, 0b11 };

#define IRLAP_DISCOVERY_TO_IRLAP(disc) (container_of((disc), struct irlap, discovery))

#define LOCAL_TAG "IRDA LAP DISCOVERY"

#define IRLAP_DISC_LOGV(disc, fmt, ...) IRHAL_LOGV(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGD(disc, fmt, ...) IRHAL_LOGD(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGI(disc, fmt, ...) IRHAL_LOGI(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGW(disc, fmt, ...) IRHAL_LOGW(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGE(disc, fmt, ...) IRHAL_LOGE(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)

static void irlap_slot_timeout(void* priv) {
  struct irlap_discovery* disc = priv;
}

static void irlap_discovery_frame_init(struct irlap* lap, union irlap_xid_frame* frame) {
  frame->fi = IRLAP_FORMAT_ID;
  frame->src_address = irlap_get_address(lap);
  frame->version = IRLAP_VERSION;
}

static int irlap_discovery_send_xid_cmd(struct irlap_discovery* disc) {
  int err;
	struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  union irlap_xid_frame frame;
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_CMD_XID | IRLAP_CMD_POLL,
  };

  irlap_discovery_frame_init(lap, &frame);
  frame.dst_address = IRLAP_ADDR_BCAST;
  frame.flags = irlap_discovery_slot_table[disc->num_slots] & IRLAP_XID_FRAME_FLAGS_MASK;
  if(disc->current_slot < disc->num_slots) {
    frame.slot = disc->current_slot;
    err = irlap_send_frame(lap, &hdr, frame.data, sizeof(frame.data));
    if(err) {
      IRLAP_DISC_LOGE(disc, "Failed to send XID discovery cmd in slot %u: %d", frame.slot, err);
      goto fail;
    }
    err = irlap_set_timer(lap, IRLAP_SLOT_TIMEOUT, irlap_slot_timeout, disc);
    if(err) {
      IRLAP_DISC_LOGE(disc, "Failed to set up discovery slot timeout after slot %u: %d", frame.slot, err);
      goto fail;
    }
    disc->current_slot++;
  } else {
    frame.slot = IRLAP_XID_SLOT_NUM_FINAL;
    err = irlap_send_frame(lap, &hdr, frame.data_info, sizeof(frame.data) + disc->discovery_info_len);
    if(err) {
      IRLAP_DISC_LOGE(disc, "Failed to send final discovery frame: %d", err);
      goto fail;
    }
    lap->state = IRLAP_STATION_MODE_NDM;
  }

  return 0;

fail:
  lap->state = IRLAP_STATION_MODE_NDM;
  return err;
}

int irlap_discovery_request(struct irlap_discovery* disc, uint8_t num_slots, uint8_t* discovery_info, uint8_t discovery_info_len) {
  int err;
	struct irlap* lap;
  if(num_slots >= IRLAP_DISCOVERY_MAX_SLOTS) {
    IRLAP_DISC_LOGE(disc, "Invalid number of discovery slots. Must be at least one and at most 16");
    err = -EINVAL;
    goto fail;
  }

  if(irlap_discovery_slot_table[num_slots] < 0) {
    IRLAP_DISC_LOGE(disc, "Invalid number of discovery slots. Only 1, 6, 8 and 16 are supported but %u was requested", num_slots);
    err = -EINVAL;
    goto fail;
  }

  if(discovery_info_len > IRLAP_DISCOVERY_INFO_MAX_LEN) {
    IRLAP_DISC_LOGE(disc, "Discovery info is too long, max length is %u but %u was requested", IRLAP_DISCOVERY_INFO_MAX_LEN, discovery_info_len);
    err = -EINVAL;
    goto fail;
  }

  disc->discovery_info_len = discovery_info_len;
  memcpy(disc->discovery_info, discovery_info, discovery_info_len);

  lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  irlap_lock_take(lap, lap->state_lock);
	if(lap->state != IRLAP_STATION_MODE_NDM) {
    IRLAP_DISC_LOGW(disc, "Station is not in NDM state, can't discover");
    err = -EAGAIN;
    goto fail_locked;
	}

	if(irlap_is_media_busy(lap)) {
    IRLAP_DISC_LOGW(disc, "Media is busy, can't discover");
		err = lap->discovery.ops.confirm(IRLAP_DISCOVERY_RESULT_MEDIA_BUSY, NULL, lap->priv);
    goto fail_locked;
	}

  lap->state = IRLAP_STATION_MODE_QUERY;
	disc->num_slots = num_slots;
	disc->current_slot = 0;
  INIT_LIST_HEAD(disc->discovery_log);

  err = irlap_discovery_send_xid_cmd(disc);
  if(err) {
    IRLAP_DISC_LOGE(disc, "Failed to send xid discovery command: %d", err);
  }

fail_locked:
  irlap_lock_put(lap, lap->state_lock);
fail:
  return err;
}
