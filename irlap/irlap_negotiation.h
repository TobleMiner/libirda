#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "irlap_defs.h"
#include "../util/util.h"

typedef uint16_t irlap_negotiation_param_t;

#define IRLAP_NEGOTIATION_PARAM_TYPE_MASK 0b10000000
#define IRLAP_NEGOTIATION_PARAM_TYPE_0    0b00000000
#define IRLAP_NEGOTIATION_PARAM_TYPE_1    0b10000000
#define IRLAP_NEGOTIATION_PARAM_IS_INDEPENDENT(param) ((((param->param_id)) & IRLAP_NEGOTIATION_PARAM_TYPE_MASK) == IRLAP_NEGOTIATION_PARAM_TYPE_1)

#define IRLAP_NEGOTIATION_PARAM_UNSET     0

#define IRLAP_NEGOTIATION_PARAM_BAUDRATE_MASK_HI 0b00000001

#define IRLAP_NEGOTIATION_BAUDRATE_2400    0b0000000000000001
#define IRLAP_NEGOTIATION_BAUDRATE_9600    0b0000000000000010
#define IRLAP_NEGOTIATION_BAUDRATE_19200   0b0000000000000100
#define IRLAP_NEGOTIATION_BAUDRATE_38400   0b0000000000001000
#define IRLAP_NEGOTIATION_BAUDRATE_57600   0b0000000000010000
#define IRLAP_NEGOTIATION_BAUDRATE_115200  0b0000000000100000
#define IRLAP_NEGOTIATION_BAUDRATE_576000  0b0000000001000000
#define IRLAP_NEGOTIATION_BAUDRATE_1152000 0b0000000010000000
#define IRLAP_NEGOTIATION_BAUDRATE_4000000 0b0000000100000000

#define IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_500_MS 0b00000001
#define IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_250_MS 0b00000010
#define IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_100_MS 0b00000100
#define IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_50_MS  0b00001000

#define IRLAP_NEGOTIATION_DATA_SIZE_MASK 0b00111111
#define IRLAP_NEGOTIATION_DATA_SIZE_64   0b00000001
#define IRLAP_NEGOTIATION_DATA_SIZE_128  0b00000010
#define IRLAP_NEGOTIATION_DATA_SIZE_256  0b00000100
#define IRLAP_NEGOTIATION_DATA_SIZE_512  0b00001000
#define IRLAP_NEGOTIATION_DATA_SIZE_1024 0b00010000
#define IRLAP_NEGOTIATION_DATA_SIZE_2048 0b00100000

#define IRLAP_NEGOTIATION_WINDOW_SIZE_MASK 0b01111111
#define IRLAP_NEGOTIATION_WINDOW_SIZE_1    0b00000001
#define IRLAP_NEGOTIATION_WINDOW_SIZE_2    0b00000010
#define IRLAP_NEGOTIATION_WINDOW_SIZE_3    0b00000100
#define IRLAP_NEGOTIATION_WINDOW_SIZE_4    0b00001000
#define IRLAP_NEGOTIATION_WINDOW_SIZE_5    0b00010000
#define IRLAP_NEGOTIATION_WINDOW_SIZE_6    0b00100000
#define IRLAP_NEGOTIATION_WINDOW_SIZE_7    0b01000000

#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_48 0b00000001
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_24 0b00000010
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_12 0b00000100
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_5  0b00001000
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_3  0b00010000
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_2  0b00100000
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_1  0b01000000
#define IRLAP_NEGOTIATION_ADDITIONAL_BOFS_0  0b10000000

#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_10000_US 0b00000001
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_5000_US  0b00000010
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_1000_US  0b00000100
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_500_US   0b00001000
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_100_US   0b00010000
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_50_US    0b00100000
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_10_US    0b01000000
#define IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_0_US     0b10000000

#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_3_S  0b00000001
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_8_S  0b00000010
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_12_S 0b00000100
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_16_S 0b00001000
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_20_S 0b00010000
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_25_S 0b00100000
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_30_S 0b01000000
#define IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_40_S 0b10000000

#define IRLAP_NEGOTIATION_PARAM_MSB (sizeof(irlap_negotiation_param_t) * 8 - 1)
#define IRLAP_NEGOTIATION_NUM_PARAMS (ARRAY_LEN(((irlap_negotiation_params_t*)NULL)->params))

union irlap_negotiation_params {
  struct {
    irlap_negotiation_param_t baudrate;
    irlap_negotiation_param_t max_turn_around_time;
    irlap_negotiation_param_t data_size;
    irlap_negotiation_param_t window_size;
    irlap_negotiation_param_t additional_bofs;
    irlap_negotiation_param_t min_turn_around_time;
    irlap_negotiation_param_t disconnect_threshold;
  } __attribute__((packed));
  irlap_negotiation_param_t params[7];
};

typedef union irlap_negotiation_params irlap_negotiation_params_t;

typedef int (*irlap_negotiation_params_update_f)(irlap_negotiation_params_t* params, uint8_t* data, uint8_t len);
typedef ssize_t (*irlap_negotiation_params_populate_f)(uint8_t* data, uint8_t len, irlap_negotiation_params_t* params);

struct irlap_negotiation_param {
  uint8_t param_id;
  uint8_t min_len;
  uint8_t max_len;
  uint8_t param_idx;
  uint8_t param_mask;
  irlap_negotiation_params_update_f update;
  irlap_negotiation_params_populate_f populate;
};

struct irlap_negotiation_base_line_capacity {
  uint32_t baudrate;
  uint32_t capacity;
};

struct irlap_negotiation_values {
  uint32_t baudrate;
  uint16_t max_turn_around_time_ms;
  uint16_t data_size;
  uint8_t  window_size;
  uint8_t  additional_bofs;
  uint16_t min_turn_around_time_us;
  uint8_t  disconnect_threshold_time_s;
};

typedef struct irlap_negotiation_values irlap_negotiation_values_t;

ssize_t irlap_negotiation_update_params(irlap_negotiation_params_t* params, uint8_t* data, size_t len);
ssize_t irlap_negotiation_populate_params(uint8_t* data, size_t len, irlap_negotiation_params_t* params);
int irlap_negotiation_merge_params(irlap_negotiation_params_t* a, irlap_negotiation_params_t* b);
int irlap_negotiation_translate_params_to_values(irlap_negotiation_values_t* values, irlap_negotiation_params_t* params);
void irlap_negotiation_load_default_params(irlap_negotiation_params_t* params);
void irlap_negotiation_load_default_values(irlap_negotiation_values_t* values);
int irlap_negotiation_translate_values_to_params(irlap_negotiation_params_t* params, irlap_negotiation_values_t* values, uint16_t baudrates);

