#pragma once

typedef uint8_t  irlap_version_t;
typedef uint32_t irlap_addr_t;
typedef uint8_t irlap_connection_addr_t;
typedef uint8_t irlap_control_t;

#define IRLAP_ADDR_BCAST 0xFFFFFFFF

#define IRLAP_CONNECTION_ADDRESS_CMD_BIT 0b00000001
#define IRLAP_CONNECTION_ADDRESS_MASK    0b11111110
#define IRLAP_CONNECTION_ADDRESS_BCAST   0b11111110
#define IRLAP_CONNECTION_ADDRESS_NULL    0b00000000

#define IRLAP_FRAME_MAKE_ADDRESS_COMMAND(addr) (((addr) & IRLAP_CONNECTION_ADDRESS_MASK) | IRLAP_CONNECTION_ADDRESS_CMD_BIT)
#define IRLAP_FRAME_MAKE_ADDRESS_RESPONSE(addr) ((addr) & IRLAP_CONNECTION_ADDRESS_MASK)

#define IRLAP_FRAME_FORMAT_MASK        0b00000011
#define IRLAP_FRAME_FORMAT_UNNUMBERED  0b00000011
#define IRLAP_FRAME_FORMAT_SUPERVISORY 0b00000001
#define IRLAP_FRAME_FORMAT_INFORMATION 0b00000000

#define IRLAP_CMD_MASK 0b11101100
#define IRLAP_CMD_SNMR 0b10000000
#define IRLAP_CMD_DISC 0b01000000
#define IRLAP_CMD_UI   0b00000000
#define IRLAP_CMD_XID  0b00101100
#define IRLAP_CMD_TEST 0b11100000

#define IRLAP_RESP_MASK 0b11101100
#define IRLAP_RESP_RNRM 0b10000000
#define IRLAP_RESP_UA   0b01100000
#define IRLAP_RESP_FRMR 0b10000100
#define IRLAP_RESP_DM   0b00001100
#define IRLAP_RESP_RD   0b01000000
#define IRLAP_RESP_UI   0b00000000
#define IRLAP_RESP_XID  0b10101100
#define IRLAP_RESP_TEST 0b11100000

typedef enum {
  IRLAP_STATION_MODE_NDM = 0,
  IRLAP_STATION_MODE_NRM,
} irlap_station_mode_t;

typedef enum {
  IRLAP_STATION_ROLE_PRIMARY,
  IRLAP_STATION_ROLE_SECONDARY,
} irlap_station_role_t;