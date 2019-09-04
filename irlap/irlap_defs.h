#pragma once

#include "../util/list.h"

struct irlap;
struct irlap_connection;
struct irlap_data_fragment;

typedef uint8_t  irlap_version_t;
typedef uint32_t irlap_addr_t;
typedef uint8_t irlap_connection_addr_t;
typedef uint8_t irlap_control_t;

#define IRLAP_VERSION 0   // Always 0, see section section 5.7.1.4.1.4.1.5 IrLAP specification v1.1
#define IRLAP_FORMAT_ID 1 // Always one, see section 5.7.1.4.1.3 IrLAP specification v1.1

#define IRLAP_MEDIA_BUSY_THRESHOLD 10

#define IRLAP_BAUDRATE_CONTENTION 9600

#define IRLAP_ERR_BASE                             0x400
#define IRLAP_ERR_ADDRESS                         (IRLAP_ERR_BASE + 1)
#define IRLAP_ERR_UNITDATA_TOO_LONG               (IRLAP_ERR_BASE + 2)
#define IRLAP_ERR_STATION_STATE                   (IRLAP_ERR_BASE + 3)
#define IRLAP_ERR_UNITDATA_TIME_LIMIT             (IRLAP_ERR_BASE + 4)
#define IRLAP_ERR_MEDIA_BUSY                      (IRLAP_ERR_BASE + 5)
#define IRLAP_ERR_NOT_IMPLEMENTED                 (IRLAP_ERR_BASE + 6)
#define IRLAP_ERR_NO_CONNECTION_ADDRESS_AVAILABLE (IRLAP_ERR_BASE + 7)
#define IRLAP_ERR_NO_COMMON_PARAMETERS_FOUND      (IRLAP_ERR_BASE + 8)
#define IRLAP_ERR_NO_CONNECTION                   (IRLAP_ERR_BASE + 9)
#define IRLAP_ERR_POLL                            (IRLAP_ERR_BASE + 10)

#define IRLAP_SLOT_TIMEOUT 50
#define IRLAP_P_TIMEOUT_MAX 500
#define IRLAP_F_TIMEOUT_MAX 500
#define IRLAP_MEDIA_BUSY_TIMEOUT 650

#define IRLAP_MAX_DATA_SIZE 2048
#define IRLAP_MAX_DATA_SIZE 2048

#define IRLAP_ADDR_BCAST 0xFFFFFFFF
#define IRLAP_ADDR_NULL  0x00000000

#define IRLAP_CONNECTION_ADDRESS_CMD_BIT 0b00000001
#define IRLAP_CONNECTION_ADDRESS_MASK    0b11111110
#define IRLAP_CONNECTION_ADDRESS_BCAST   0b11111110
#define IRLAP_CONNECTION_ADDRESS_NULL    0b00000000

#define IRLAP_CONNECTION_ADDRESS_MIN    0b00000001
#define IRLAP_CONNECTION_ADDRESS_MAX    IRLAP_CONNECTION_ADDRESS_MASK

#define IRLAP_FRAME_IS_BCAST(hdr) (((hdr)->connection_address & IRLAP_CONNECTION_ADDRESS_MASK) == IRLAP_CONNECTION_ADDRESS_BCAST)
#define IRLAP_FRAME_IS_COMMAND(hdr) (((hdr)->connection_address & IRLAP_CONNECTION_ADDRESS_CMD_BIT) == IRLAP_CONNECTION_ADDRESS_CMD_BIT)
#define IRLAP_FRAME_IS_RESPONSE(hdr) (((hdr)->connection_address & IRLAP_CONNECTION_ADDRESS_CMD_BIT) == 0)

#define IRLAP_FRAME_MAKE_ADDRESS_COMMAND(addr) (((addr) & IRLAP_CONNECTION_ADDRESS_MASK) | IRLAP_CONNECTION_ADDRESS_CMD_BIT)
#define IRLAP_FRAME_MAKE_ADDRESS_RESPONSE(addr) ((addr) & IRLAP_CONNECTION_ADDRESS_MASK)

#define IRLAP_CONNECTION_ADDRESS_MASK_CMD_BIT(addr) ((addr) & IRLAP_CONNECTION_ADDRESS_MASK)

#define IRLAP_FRAME_FORMAT_MASK        0b00000011
#define IRLAP_FRAME_FORMAT_UNNUMBERED  0b00000011
#define IRLAP_FRAME_FORMAT_SUPERVISORY 0b00000001
#define IRLAP_FRAME_FORMAT_INFORMATION 0b00000000

#define IRLAP_FRAME_IS_UNNUMBERED(hdr) (((hdr)->control & IRLAP_FRAME_FORMAT_MASK) == IRLAP_FRAME_FORMAT_UNNUMBERED)

#define IRLAP_FRAME_ADDITIONAL_BOF_CONTENTION 10
#define IRLAP_FRAME_ADDITIONAL_BOF_MAX        48

#define IRLAP_FRAME_POLL_FINAL 0b00010000
#define IRLAP_FRAME_IS_POLL_FINAL(hdr) (((hdr)->control & IRLAP_FRAME_POLL_FINAL) == IRLAP_FRAME_POLL_FINAL)
#define IRLAP_FRAME_MASK_POLL_FINAL(ctl) ((ctl) & ~IRLAP_FRAME_POLL_FINAL)

#define IRLAP_SUPERVISORY_NR_MASK 0b11100000

#define IRLAP_CMD_MASK 0b11101100
#define IRLAP_CMD_SNRM 0b10000000
#define IRLAP_CMD_DISC 0b01000000
#define IRLAP_CMD_UI   0b00000000
#define IRLAP_CMD_XID  0b00101100
#define IRLAP_CMD_TEST 0b11100000
#define IRLAP_CMD_POLL 0b00010000
#define IRLAP_CMD_RR   0b00000000
#define IRLAP_CMD_RNR  0b00000100
#define IRLAP_CMD_REJ  0b00001000
#define IRLAP_CMD_SREJ 0b00001100

#define IRLAP_RESP_MASK  0b11101100
#define IRLAP_RESP_RNRM  0b10000000
#define IRLAP_RESP_UA    0b01100000
#define IRLAP_RESP_FRMR  0b10000100
#define IRLAP_RESP_DM    0b00001100
#define IRLAP_RESP_RD    0b01000000
#define IRLAP_RESP_UI    0b00000000
#define IRLAP_RESP_XID   0b10101100
#define IRLAP_RESP_TEST  0b11100000
#define IRLAP_RESP_FINAL 0b00010000
#define IRLAP_CMD_RR     0b00000000
#define IRLAP_CMD_RNR    0b00000100
#define IRLAP_CMD_REJ    0b00001000
#define IRLAP_CMD_SREJ   0b00001100

#define IRLAP_
typedef enum {
  IRLAP_STATION_MODE_NDM = 0,
  IRLAP_STATION_MODE_NRM,
  IRLAP_STATION_MODE_QUERY,
  IRLAP_STATION_MODE_REPLY,
  IRLAP_STATION_MODE_SCONN,
  IRLAP_STATION_MODE_SSETUP,
} irlap_station_mode_t;

#define IRLAP_STATE_IS_CONTENTION(state) ( \
  ((state) == IRLAP_STATION_MODE_NDM)  || \
  ((state) == IRLAP_STATION_MODE_QUERY) || \
  ((state) == IRLAP_STATION_MODE_REPLY) \
)

typedef enum {
  IRLAP_STATION_ROLE_NONE = 0,
  IRLAP_STATION_ROLE_PRIMARY,
  IRLAP_STATION_ROLE_SECONDARY,
} irlap_station_role_t;

typedef enum {
  IRLAP_CONNECTION_STATE_SETUP = 0,
  IRLAP_CONNECTION_STATE_RECV,
} irlap_connection_state_t;

#define IRLAP_CONNECTION_IS_NEGOTIATED(conn) ( \
  ((conn)->connection_state != IRLAP_CONNECTION_STATE_SETUP) \
)

#define IRLAP_FRAME_HANDLED     0
#define IRLAP_FRAME_NOT_HANDLED 1

#define IRLAP_INDIRECTION_DISCOVERY_BUSY 0

typedef struct list_head irlap_connection_list_t;
