#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "irlap_discovery.h"
#include "irlap.h"
#include "../util/util.h"

static int8_t irlap_discovery_slot_table[17] = { -1, 0b00, -1, -1, -1, -1, 0b01, -1, 0b10, -1, -1, -1, -1, -1, -1, -1, 0b11 };
static uint8_t irlap_discovery_slot_reverse_table[4] = { 1, 6, 8, 16 };

#define IRLAP_DISCOVERY_TO_IRLAP(disc) (container_of((disc), struct irlap, discovery))

#define LOCAL_TAG "IRDA LAP DISCOVERY"

#define IRLAP_DISC_LOGV(disc, fmt, ...) IRHAL_LOGV(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGD(disc, fmt, ...) IRHAL_LOGD(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGI(disc, fmt, ...) IRHAL_LOGI(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGW(disc, fmt, ...) IRHAL_LOGW(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_DISC_LOGE(disc, fmt, ...) IRHAL_LOGE(IRLAP_DISCOVERY_TO_IRLAP(disc)->phy->hal, fmt, ##__VA_ARGS__)

static int irlap_discovery_send_xid_cmd(struct irlap_discovery* disc);

int irlap_discovery_init(struct irlap_discovery* disc) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  int err = irlap_lock_alloc(lap, &disc->discovery_log_final_lock);
  if(err) {
    IRLAP_DISC_LOGE(disc, "Failed to allocate discovery log final lock");
    goto fail;
  }

  return 0;

fail:
  return err;
}

void irlap_discovery_free(struct irlap_discovery* disc) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  irlap_lock_free(lap, &disc->discovery_log_final_lock);
}

static void irlap_slot_timeout(void* priv) {
  struct irlap_discovery* disc = priv;
  disc->slot_timer = 0;
  irlap_discovery_send_xid_cmd(disc);
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
  frame.flags = irlap_discovery_slot_table[disc->num_slots];
  if(disc->conflict_address == IRLAP_ADDR_NULL) {
    frame.dst_address = IRLAP_ADDR_BCAST;
  } else {
    frame.dst_address = disc->conflict_address;
    frame.flags |= IRLAP_XID_FRAME_FLAGS_GENERATE_NEW_ADDRESS;
  }
  frame.flags &= IRLAP_XID_FRAME_FLAGS_MASK;
  if(disc->current_slot < disc->num_slots) {
    frame.slot = disc->current_slot;
    err = irlap_send_frame(lap, &hdr, frame.data, sizeof(frame.data));
    if(err) {
      IRLAP_DISC_LOGE(disc, "Failed to send XID discovery cmd in slot %u: %d", frame.slot, err);
      goto fail;
    }
    err = irlap_set_timer(lap, IRLAP_SLOT_TIMEOUT, irlap_slot_timeout, disc);
    if(err < 0) {
      IRLAP_DISC_LOGE(disc, "Failed to set up discovery slot timeout after slot %u: %d", frame.slot, err);
      goto fail;
    }
    disc->slot_timer = err;
    disc->current_slot++;
  } else {
    frame.slot = IRLAP_XID_SLOT_NUM_FINAL;
    memcpy(frame.discovery_info, disc->discovery_info, disc->discovery_info_len);
    err = irlap_send_frame(lap, &hdr, frame.data_info, sizeof(frame.data) + disc->discovery_info_len);
    if(err) {
      IRLAP_DISC_LOGE(disc, "Failed to send final discovery frame: %d", err);
      goto fail;
    }
    irlap_lock_take(lap, disc->discovery_log_final_lock);
    list_replace(&disc->discovery_log, &disc->discovery_log_final);
    irlap_lock_take_reentrant(lap, lap->state_lock);
    lap->state = IRLAP_STATION_MODE_NDM;
    irlap_lock_put_reentrant(lap, lap->state_lock);
    if(disc->conflict_address == IRLAP_ADDR_NULL) {
      if(disc->discovery_ops.confirm) {
        disc->discovery_ops.confirm(IRLAP_DISCOVERY_RESULT_OK, &disc->discovery_log_final, lap->priv);
      }
    } else {
      if(disc->new_address_ops.confirm) {
        disc->new_address_ops.confirm(IRLAP_DISCOVERY_RESULT_OK, &disc->discovery_log_final, lap->priv);
      }
    }
    {
      irlap_discovery_log_list_t *cursor, *next;
      LIST_FOR_EACH_SAFE(cursor, next, &disc->discovery_log_final) {
        struct irlap_discovery_log_entry* entry = LIST_GET_ENTRY(cursor, struct irlap_discovery_log_entry, list);
        free(entry);
      }
    }
    irlap_lock_put(lap, disc->discovery_log_final_lock);
  }

  return 0;

fail:
  irlap_lock_take_reentrant(lap, lap->state_lock);
  lap->state = IRLAP_STATION_MODE_NDM;
  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

void irlap_discovery_indirect_busy(struct irlap* lap, void* data) {
  bool is_null_addr = !!(int)data;
  struct irlap_discovery* disc = &lap->discovery;
  if(is_null_addr) {
    if(disc->discovery_ops.confirm) {
      disc->discovery_ops.confirm(IRLAP_DISCOVERY_RESULT_MEDIA_BUSY, NULL, lap->priv);
    }
  } else {
    if(disc->new_address_ops.confirm) {
      disc->new_address_ops.confirm(IRLAP_DISCOVERY_RESULT_MEDIA_BUSY, NULL, lap->priv);
    }
  }
}

static int irlap_discovery_request_(struct irlap_discovery* disc, uint8_t num_slots, uint8_t* discovery_info, uint8_t discovery_info_len, irlap_addr_t new_addr) {
  int err = 0;
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
  irlap_lock_take_reentrant(lap, lap->state_lock);
  if(lap->state != IRLAP_STATION_MODE_NDM) {
    IRLAP_DISC_LOGW(disc, "Station is not in NDM state, can't discover");
    err = -EAGAIN;
    goto fail_locked;
  }

  if(irlap_is_media_busy(lap)) {
    IRLAP_DISC_LOGW(disc, "Media is busy, can't discover");
    err = irlap_indirect_call(lap, IRLAP_INDIRECTION_DISCOVERY_BUSY, (void*)(new_addr == IRLAP_ADDR_NULL));
    goto fail_locked;
  }

  lap->state = IRLAP_STATION_MODE_QUERY;
  disc->num_slots = num_slots;
  disc->current_slot = 0;
  INIT_LIST_HEAD(disc->discovery_log);

  disc->conflict_address = new_addr;

  err = irlap_discovery_send_xid_cmd(disc);
  if(err) {
    IRLAP_DISC_LOGE(disc, "Failed to send xid discovery command: %d", err);
  }

fail_locked:
  irlap_lock_put_reentrant(lap, lap->state_lock);
fail:
  return err;
}

int irlap_new_address_request(struct irlap_discovery* disc, uint8_t num_slots, uint8_t* discovery_info, uint8_t discovery_info_len, irlap_addr_t conflict_addr) {
  return irlap_discovery_request_(disc, num_slots, discovery_info, discovery_info_len, conflict_addr);
}

int irlap_discovery_request(struct irlap_discovery* disc, uint8_t num_slots, uint8_t* discovery_info, uint8_t discovery_info_len) {
  return irlap_discovery_request_(disc, num_slots, discovery_info, discovery_info_len, IRLAP_ADDR_NULL);
}

static int irlap_discovery_validate_xid_frame(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t* data, size_t len) {
  // Ensure all required fields are present
  if(len < IRLAP_XID_FRAME_MIN_SIZE) {
    IRLAP_DISC_LOGD(disc, "Got invalid xid frame, frame too short %zu bytes < %zu bytes", len, IRLAP_XID_FRAME_MIN_SIZE);
    return -EINVAL;
  }
  // Ensure frame can't overflow xid frame buffer
  if(len > IRLAP_XID_FRAME_MAX_SIZE) {
    IRLAP_DISC_LOGD(disc, "Got invalid xid frame, frame too long %zu bytes > %zu bytes", len, IRLAP_XID_FRAME_MAX_SIZE);
    return -EINVAL;
  }

  memcpy(frame->data_info, data, len);

  // Specification lists only one valid format id
  if(frame->fi != IRLAP_FORMAT_ID) {
    IRLAP_DISC_LOGD(disc, "Got invalid xid frame, invalid format id '%u', must be '%u'", frame->fi, IRLAP_FORMAT_ID);
    return -EINVAL;
  }

  // There is only one irlap version
  if(frame->version != IRLAP_VERSION) {
    IRLAP_DISC_LOGD(disc, "Got invalid xid frame, invalid version number '%u', must be '%u'", frame->version, IRLAP_VERSION);
    return -EINVAL;
  }

  // Source address may neither be NULL nor broadcast address
  if(frame->src_address == IRLAP_ADDR_NULL) {
    IRLAP_DISC_LOGD(disc, "Got invalid xid frame, NULL source address is invalid");
    return -EINVAL;
  }
  if(frame->src_address == IRLAP_ADDR_BCAST) {
    IRLAP_DISC_LOGD(disc, "Got invalid xid frame, broadcast source address is invalid");
    return -EINVAL;
  }
  
  return len - IRLAP_XID_FRAME_MIN_SIZE;
}

static void irlap_query_timeout(void* priv) {
  struct irlap_discovery* disc = priv;
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  disc->query_timer = 0;
  irlap_lock_take_reentrant(lap, lap->state_lock);
  lap->state = IRLAP_STATION_MODE_NDM;
  irlap_lock_put_reentrant(lap, lap->state_lock);
}

static int irlap_discovery_send_xid_response_discovery(struct irlap_discovery* disc, union irlap_xid_frame* query_frame) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  union irlap_xid_frame frame;
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_RESPONSE(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | IRLAP_RESP_XID | IRLAP_RESP_FINAL,
  };
  int err;

  IRLAP_DISC_LOGD(disc, "Sending discovery response on slot: %u", query_frame->slot);
  irlap_discovery_frame_init(lap, &frame);
  frame.dst_address = query_frame->src_address;
  frame.flags = query_frame->flags & IRLAP_XID_FRAME_FLAGS_MASK;
  frame.slot = query_frame->slot;
  err = irlap_send_frame(lap, &hdr, frame.data_info, sizeof(frame.data));
  if(err) {
    IRLAP_DISC_LOGE(disc, "Failed to send discovery response: %d", err);
    return err;
  }
  disc->frame_sent = true;
  return 0;
}

static int irlap_discovery_handle_xid_cmd_discovery_initial(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t num_slots) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  int err;
  if(frame->slot == IRLAP_XID_SLOT_NUM_FINAL) {
    return IRLAP_FRAME_HANDLED;
  }

  if(frame->flags & IRLAP_XID_FRAME_FLAGS_GENERATE_NEW_ADDRESS) {
    err = irlap_regenerate_address(lap);
    if(err) {
      IRLAP_DISC_LOGW(disc, "Failed to generat new station address");
      return err;
    }
  }

  err = irlap_random_u8(lap, &disc->slot, frame->slot, num_slots);
  if(err) {
    IRLAP_DISC_LOGW(disc, "Failed to get random slot number");
    return err;
  }

  IRLAP_DISC_LOGD(disc, "Will send xid discovery response on slot %u", disc->slot);

  disc->frame_sent = false;
  if(frame->slot == disc->slot) {
    irlap_discovery_send_xid_response_discovery(disc, frame);
  }

  err = irlap_set_timer(lap, IRLAP_SLOT_TIMEOUT * ((uint16_t)(num_slots - frame->slot)), irlap_query_timeout, disc);
  if(err < 0) {
    IRLAP_DISC_LOGW(disc, "Failed to timer for query timeout");
    return err;
  }
  disc->query_timer = err;

  lap->state = IRLAP_STATION_MODE_REPLY;
  return IRLAP_FRAME_HANDLED;
}

static int irlap_discovery_handle_xid_cmd_discovery_additional(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t discovery_info_len) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  if(frame->slot == IRLAP_XID_SLOT_NUM_FINAL) {
    struct irlap_discovery_log log = {
      .solicited = false,
      .sniff = false,
      .device_address = frame->src_address,
      .irlap_version = frame->version,
      .discovery_info.len = discovery_info_len
    };
    irlap_clear_timer(lap, disc->query_timer);
    lap->state = IRLAP_STATION_MODE_NDM;
    if(disc->discovery_ops.indication) {
      memcpy(log.discovery_info.data, frame->discovery_info, discovery_info_len);
      disc->discovery_ops.indication(&log, lap->priv);
    }
    return IRLAP_FRAME_HANDLED;
  }

  if(disc->frame_sent) {
    return IRLAP_FRAME_HANDLED;
  }

  if(frame->slot < disc->slot) {
    return IRLAP_FRAME_HANDLED;
  }

  return irlap_discovery_send_xid_response_discovery(disc, frame);
}

static int irlap_discovery_handle_xid_cmd_discovery(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t discovery_info_len) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  int err = IRLAP_FRAME_HANDLED;
  uint8_t num_slots = irlap_discovery_slot_reverse_table[frame->flags & IRLAP_XID_FRAME_FLAGS_SLOT_MASK];

  if(frame->dst_address != IRLAP_ADDR_BCAST && frame->dst_address != lap->address) {
    IRLAP_DISC_LOGV(disc, "Got xid discovery frame not addressed to us, dst: %08x", frame->dst_address);
    goto fail;
  }

  if(frame->slot >= num_slots && frame->slot != IRLAP_XID_SLOT_NUM_FINAL) {
    IRLAP_DISC_LOGD(disc, "Got xid discovery frame with slot num %u >= max slot num %u", frame->slot, num_slots);
    err = -EINVAL;
    goto fail;
  }

  irlap_lock_take_reentrant(lap, lap->state_lock);
  switch(lap->state) {
    case IRLAP_STATION_MODE_NDM:
      IRLAP_DISC_LOGD(disc, "Starting xid discovery response");
      err = irlap_discovery_handle_xid_cmd_discovery_initial(disc, frame, num_slots);
      break;
    case IRLAP_STATION_MODE_REPLY:
      IRLAP_DISC_LOGD(disc, "Continuing xid discovery response");
      err = irlap_discovery_handle_xid_cmd_discovery_additional(disc, frame, discovery_info_len);
      break;
    default:
      IRLAP_DISC_LOGD(disc, "Station neither in NDM nor REPLY mode, can't respond to discovery");
      err = -EAGAIN;
      goto fail_locked;
  }

fail_locked:
  irlap_lock_put_reentrant(lap, lap->state_lock);
fail:
  return err;
}

int irlap_discovery_handle_xid_cmd(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool poll) {
  struct irlap_discovery* disc = &lap->discovery;
  int err;
  union irlap_xid_frame frame;
  uint8_t discovery_info_len;

  IRLAP_DISC_LOGD(disc, "Got xid cmd");
  err = irlap_discovery_validate_xid_frame(disc, &frame, data, len);
  if(err < 0) {
    IRLAP_DISC_LOGD(disc, "Xid cmd is invalid");
    return err;
  }
  discovery_info_len = err;

  return irlap_discovery_handle_xid_cmd_discovery(disc, &frame, discovery_info_len);
}

static int irlap_discovery_handle_xid_resp_discovery_ndm(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t discovery_info_len) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);
  struct irlap_discovery_log log;

  if(!IRLAP_FRAME_IS_SNIFF(frame)) {
    IRLAP_DISC_LOGW(disc, "Refusing to handle discovery response in ndm mode");
    return IRLAP_FRAME_NOT_HANDLED;
  }

  log.solicited = false;
  log.sniff = true;
  log.device_address = frame->src_address;
  log.irlap_version = frame->version;
  log.discovery_info.len = discovery_info_len;
  memcpy(log.discovery_info.data, frame->discovery_info, discovery_info_len);

  if(disc->discovery_ops.indication) {
    disc->discovery_ops.indication(&log, lap->priv);
  }

  return IRLAP_FRAME_HANDLED;
}

static int irlap_discovery_handle_xid_resp_discovery_query(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t discovery_info_len) {
  struct irlap_discovery_log_entry* entry;

  if(IRLAP_FRAME_IS_SNIFF(frame)) {
    IRLAP_DISC_LOGW(disc, "Refusing to handle sniff xid response in query mode");
    return IRLAP_FRAME_NOT_HANDLED;
  }

  entry = calloc(1, sizeof(struct irlap_discovery_log_entry));
  if(!entry) {
    IRLAP_DISC_LOGW(disc, "Failed to allocate memory for query log entry");
    return -ENOMEM;
  }

  entry->discovery_log.solicited = true;
  entry->discovery_log.sniff = IRLAP_FRAME_IS_SNIFF(frame);
  entry->discovery_log.device_address = frame->src_address;
  entry->discovery_log.irlap_version = frame->version;
  entry->discovery_log.discovery_info.len = discovery_info_len;
  memcpy(entry->discovery_log.discovery_info.data, frame->discovery_info, discovery_info_len);

  LIST_APPEND(&entry->list, &disc->discovery_log);

  return IRLAP_FRAME_HANDLED;
}

static int irlap_discovery_handle_xid_resp_discovery_sconn(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t discovery_info_len) {
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);

  if(!IRLAP_FRAME_IS_SNIFF(frame)) {
    IRLAP_DISC_LOGW(disc, "Refusing to handle discovery response in sconn mode");
    return IRLAP_FRAME_NOT_HANDLED;
  }

  return irlap_connect_handle_sniff_xid_req_sconn(lap, frame->src_address);
}

static int irlap_discovery_handle_xid_resp_discovery(struct irlap_discovery* disc, union irlap_xid_frame* frame, uint8_t discovery_info_len) {
  int err = IRLAP_FRAME_HANDLED;
  struct irlap* lap = IRLAP_DISCOVERY_TO_IRLAP(disc);

  irlap_lock_take_reentrant(lap, lap->state_lock);
  switch(lap->state) {
    case IRLAP_STATION_MODE_NDM:
      err = irlap_discovery_handle_xid_resp_discovery_ndm(disc, frame, discovery_info_len);
      break;
    case IRLAP_STATION_MODE_QUERY:
      err = irlap_discovery_handle_xid_resp_discovery_query(disc, frame, discovery_info_len);
      break;
    case IRLAP_STATION_MODE_SCONN:
      err = irlap_discovery_handle_xid_resp_discovery_sconn(disc, frame, discovery_info_len);
      break;
    default:
      IRLAP_DISC_LOGD(disc, "Station neither in ndm nor query nor sconn mode, ignoring xid discovery response");
      err = -EAGAIN;
  }

  irlap_lock_put_reentrant(lap, lap->state_lock);
  return err;
}

int irlap_discovery_handle_xid_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool final) {
  struct irlap_discovery* disc = &lap->discovery;
  int err;
  union irlap_xid_frame frame;
  uint8_t discovery_info_len;

  IRLAP_DISC_LOGD(disc, "Got xid resp");
  err = irlap_discovery_validate_xid_frame(disc, &frame, data, len);
  if(err < 0) {
    IRLAP_DISC_LOGD(disc, "Xid resp is invalid");
    return err;
  }
  discovery_info_len = err;

  return irlap_discovery_handle_xid_resp_discovery(disc, &frame, discovery_info_len);
}
