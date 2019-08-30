#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../util/list.h"
#include "irlap_defs.h"

typedef struct list_head irlap_discovery_log_list_t;

#define IRLAP_DISCOVERY_MAX_SLOTS 16
#define IRLAP_DISCOVERY_INFO_MAX_LEN 32

typedef enum {
  IRLAP_DISCOVERY_RESULT_OK = 0,
  IRLAP_DISCOVERY_RESULT_MEDIA_BUSY,
  IRLAP_DISCOVERY_RESULT_CANCELED,
} irlap_discovery_result_t;

struct irlap_discovery_log {
  bool            solicited;
  bool            sniff;
  irlap_addr_t    device_address;
  irlap_version_t irlap_version;
  struct {
    char data[IRLAP_DISCOVERY_INFO_MAX_LEN];
    uint8_t len;
  }               discovery_info;
};

struct irlap_discovery_log_entry {
  irlap_discovery_log_list_t list;
  struct irlap_discovery_log discovery_log;
};


typedef int (*irlap_discovery_indication)(struct irlap_discovery_log* log, void* priv);
typedef int (*irlap_discovery_confirm)(irlap_discovery_result_t result, irlap_discovery_log_list_t* list, void* priv);

struct irlap_discovery_ops {
  irlap_discovery_indication indication;
  irlap_discovery_confirm confirm;
};

typedef int (*irlap_new_address_confirm)(irlap_discovery_result_t result, irlap_discovery_log_list_t* list, void* priv);

struct irlap_new_address_ops {
  irlap_new_address_confirm confirm;
};

struct irlap_discovery {
  struct irlap_discovery_ops discovery_ops;
  struct irlap_new_address_ops new_address_ops;
  uint8_t num_slots;
  uint8_t current_slot;
  int slot_timer;
  int query_timer;
  uint8_t discovery_info[IRLAP_DISCOVERY_INFO_MAX_LEN];
  uint8_t discovery_info_len;
  irlap_discovery_log_list_t discovery_log;
  irlap_discovery_log_list_t discovery_log_final;
  void* discovery_log_final_lock;
  uint8_t slot;
  bool frame_sent;
  irlap_addr_t conflict_address;
};

#define IRLAP_XID_FRAME_FLAGS_MASK                 0b00000111
#define IRLAP_XID_FRAME_FLAGS_SLOT_MASK            0b00000011
#define IRLAP_XID_FRAME_FLAGS_1_SLOT               0b00000000
#define IRLAP_XID_FRAME_FLAGS_6_SLOTS              0b00000001
#define IRLAP_XID_FRAME_FLAGS_8_SLOTS              0b00000010
#define IRLAP_XID_FRAME_FLAGS_16_SLOTS             0b00000011
#define IRLAP_XID_FRAME_FLAGS_GENERATE_NEW_ADDRESS 0b00000100

#define IRLAP_XID_SLOT_NUM_FINAL 0xFF

#define IRLAP_FRAME_IS_XID_CMD(hdr) ((hdr)->control & IRLAP_CMD_MASK == IRLAP_CMD_XID)
#define IRLAP_FRAME_IS_XID_RESP(hdr) ((hdr)->control & IRLAP_RESP_MASK == IRLAP_RESP_XID)
#define IRLAP_FRAME_IS_XID(hdr) (IRLAP_FRAME_IS_XID_CMD((hdr)) || IRLAP_FRAME_IS_XID_RESP((hdr)))
#define IRLAP_FRAME_IS_SNIFF(frm) ((frm)->dst_address == IRLAP_ADDR_BCAST)

union irlap_xid_frame {
  struct {
    uint8_t fi;
    irlap_addr_t src_address;
    irlap_addr_t dst_address;
    uint8_t flags;
    uint8_t slot;
    uint8_t version;
    uint8_t discovery_info[IRLAP_DISCOVERY_INFO_MAX_LEN];
  } __attribute__((packed));
  uint8_t data[12];
  uint8_t data_info[12 + IRLAP_DISCOVERY_INFO_MAX_LEN];
};

#define IRLAP_XID_FRAME_MIN_SIZE (sizeof(((union irlap_xid_frame*)NULL)->data))
#define IRLAP_XID_FRAME_MAX_SIZE (sizeof(((union irlap_xid_frame*)NULL)->data_info))

int irlap_discovery_init(struct irlap_discovery* disc);
void irlap_discovery_free(struct irlap_discovery* disc);
int irlap_discovery_request(struct irlap_discovery* disc, uint8_t num_slots, uint8_t* discovery_info, uint8_t discovery_info_len);
int irlap_new_address_request(struct irlap_discovery* disc, uint8_t num_slots, uint8_t* discovery_info, uint8_t discovery_info_len, irlap_addr_t conflict_addr);
int irlap_discovery_handle_xid_cmd(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool poll);
int irlap_discovery_handle_xid_resp(struct irlap* lap, struct irlap_connection* conn, uint8_t* data, size_t len, bool final);

void irlap_discovery_indirect_busy(struct irlap* lap, void* data);
