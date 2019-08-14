#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "irlap.h"
#include "irlap_frame_wrapper.h"
#include "../irhal/irhal.h"
#include "../util/crc.h"

#define LOCAL_TAG "IRDA LAP"

#define IRLAP_LOGV(lap, fmt, ...) IRHAL_LOGV(lap->phy->hal, fmt, ##__VA_ARGS__)
#define IRLAP_LOGD(lap, fmt, ...) IRHAL_LOGD(lap->phy->hal, fmt, ##__VA_ARGS__)

int irlap_init(struct irlap* lap, struct irphy* phy, struct irlap_ops* ops) {
  int err;
  memset(lap, 0, sizeof(*lap));
  lap->phy = phy;
  lap->ops = *ops;

  err = irlap_regenerate_address(lap);
  if(err) {
    return err;
  }
  return 0;
}

int irlap_regenerate_address(struct irlap* lap) {
  int err;
  irlap_addr_t addr;
  err = irhal_random_bytes(lap->phy->hal, &addr, sizeof(addr));
  if(err) {
    return err;
  }
  lap->address = addr;
  return 0;
}

int irlap_make_wrapped_frame(struct irlap* lap, irlap_frame_hdr_t* hdr, uint8_t* payload, size_t payload_len) {
  return 0;
}

int irlap_send_xir(struct irlap* lap) {
  union {
    struct {
      uint8_t fi;
      uint32_t s_addr;
      uint32_t d_addr;
      uint8_t flags;
      uint8_t slot_num;
      uint8_t version;
    } __attribute__((packed));
    uint8_t data[12];
  } xid_discovery;

  xid_discovery.fi = 1;
  xid_discovery.s_addr = lap->address;
  xid_discovery.d_addr = IRLAP_ADDR_BCAST;
  xid_discovery.flags = 1;
  xid_discovery.slot_num = 0;
  xid_discovery.version = 0;
  
  irlap_frame_hdr_t hdr = {
    .connection_address = IRLAP_FRAME_MAKE_ADDRESS_COMMAND(IRLAP_CONNECTION_ADDRESS_BCAST),
    .control = IRLAP_FRAME_FORMAT_UNNUMBERED | 0b00111100,
  };

  IRLAP_LOGD(lap, "Sending XIR command, data_len: %zu", sizeof(xid_discovery.data));
  ssize_t frame_size = irlap_wrapper_get_wrapped_size(IRLAP_FRAME_WRAPPER_ASYNC, &hdr, xid_discovery.data, sizeof(xid_discovery.data), 10);
  if(frame_size < 0) {
    IRLAP_LOGD(lap, "Failed to get wrapped frame size: %d", frame_size);
    return frame_size;
  }

  IRLAP_LOGD(lap, "Wrapped frame length: %zd", frame_size);
  uint8_t* wrapped = malloc(frame_size);
  if(!wrapped) {
    return -ENOMEM;
  }

  IRLAP_LOGD(lap, "Wrapping frame");
  ssize_t res = irlap_wrapper_wrap(IRLAP_FRAME_WRAPPER_ASYNC, wrapped, frame_size, &hdr, xid_discovery.data, sizeof(xid_discovery.data), 10);
  if(res < 0) {
    goto fail;
  }

  
  IRLAP_LOGD(lap, "==== Wrapped frame START ====");
  for(size_t i = 0; i < frame_size; i++) {
    printf("%02x ", wrapped[i]);
  }
  printf("\n");
  IRLAP_LOGD(lap, "==== Wrapped frame END ====");

  IRLAP_LOGD(lap, "Wrapped frame has size of %zd bytes", res);
  irphy_tx_enable(lap->phy);
  irphy_set_baudrate(lap->phy, 9600);
  irphy_tx(lap->phy, wrapped, res);
  irphy_tx_wait(lap->phy);

  res = 0;
fail:
  free(wrapped);
  return res;
}
