#include <errno.h>
#include <stdbool.h>

#include "irlap_negotiation.h"

static int update_baudrate(irlap_negotiation_params_t* params, uint8_t* data, uint8_t len) {
  params->baudrate = *data;
  if(len == 2) {
    data++;
    params->baudrate |= (*data & IRLAP_NEGOTIATION_PARAM_BAUDRATE_MASK_HI) << 8;
  }
  return 0;
}

#define IS_EXTENDED_BAUDRATE(baudrate) (((baudrate) >> 8) & IRLAP_NEGOTIATION_PARAM_BAUDRATE_MASK_HI)

static ssize_t populate_baudrate(uint8_t* data, uint8_t len, irlap_negotiation_params_t* params) {
  if(IS_EXTENDED_BAUDRATE(params->baudrate) && len < 2) {
    return -ENOBUFS;
  }
  *data = (uint8_t)(params->baudrate & 0xFF);
  if(IS_EXTENDED_BAUDRATE(params->baudrate)) {
    data++;
    *data = (uint8_t)(params->baudrate >> 8) & IRLAP_NEGOTIATION_PARAM_BAUDRATE_MASK_HI;
    return 2;
  }
  return 1;
}

static int update_param(irlap_negotiation_params_t* params, struct irlap_negotiation_param* param, uint8_t* data, size_t len) {
  if(len < param->min_len) {
    return -ENOBUFS;
  }
  if(len > param->max_len) {
    return -EINVAL;
  }
  if(param->update) {
    return param->update(params, data, len);
  }
  params->params[param->param_idx] = *data & param->param_mask;
  return 0;
}

static ssize_t populate_param(uint8_t* data, size_t len, irlap_negotiation_params_t* params, struct irlap_negotiation_param* param) {
  if(params->params[param->param_idx] == IRLAP_NEGOTIATION_PARAM_UNSET) {
    return 0;
  }
  if(len < param->min_len) {
    return -ENOBUFS;
  }
  if(param->populate) {
    return param->update(params, data, len);
  }
  *data = params->params[param->param_idx] & param->param_mask;
  return 1;
}

static struct irlap_negotiation_param negotiation_params[] = {
  { 0x01, 1, 2, 0, 0, update_baudrate, populate_baudrate },
  { 0x82, 1, 1, 1, 0b00001111, NULL, NULL },
  { 0x83, 1, 1, 2, 0b00111111, NULL, NULL },
  { 0x84, 1, 1, 3, 0b01111111, NULL, NULL },
  { 0x85, 1, 1, 4, 0b11111111, NULL, NULL },
  { 0x86, 1, 1, 5, 0b11111111, NULL, NULL },
  { 0x08, 1, 1, 6, 0b11111111, NULL, NULL },
  { 0x00, 0, 0, 0, 0, NULL, NULL },
};

static struct irlap_negotiation_param* get_param_by_id(uint8_t id) {
  struct irlap_negotiation_param* param = negotiation_params;
  while(param->max_len != 0) {
    if(param->param_id == id) {
      return param;
    }
    param++;
  }
  return NULL;
}

static struct irlap_negotiation_param* get_param_by_index(uint8_t idx) {
  struct irlap_negotiation_param* param = negotiation_params;
  while(param->max_len != 0) {
    if(param->param_idx == idx) {
      return param;
    }
    param++;
  }
  return NULL;
}

ssize_t irlap_negotiation_update_params(irlap_negotiation_params_t* params, uint8_t* data, size_t len) {
  size_t bytes_processed = 0;
  while(len >= 2) {
    struct irlap_negotiation_param* param;
    uint8_t param_id = *data++;
    len--;
    bytes_processed++;
    uint8_t param_len = *data++;
    len--;
    bytes_processed++;
    if(param_len > len) {
      return -EINVAL;
    }
    param = get_param_by_id(param_id);
    if(param) {
      int err = update_param(params, param, data, len);
      if(err) {
        return err;
      }
    }
    data += param_len;
    len -= param_len;
    bytes_processed += param_len;
  }
  return bytes_processed;
}

ssize_t irlap_negotiation_populate_params(irlap_negotiation_params_t* params, uint8_t* data, size_t len) {
  size_t len_populated = 0;
  ssize_t err;
  int i;
  for(i = 0; i < IRLAP_NEGOTIATION_NUM_PARAMS; i++) {
    struct irlap_negotiation_param* param = get_param_by_index(i);
    if(!param) {
      continue;
    }
    err = populate_param(data, len, params, param);
    if(err < 0) {
      return err;
    }
    len_populated += err;
    data += err;
  }
  return len_populated;
}

static irlap_negotiation_param_t get_most_significant_set_bit(irlap_negotiation_param_t val) {
  irlap_negotiation_param_t mask = 1 << IRLAP_NEGOTIATION_PARAM_MSB;
  while(mask > 0) {
    if(val & mask) {
      return val & mask;
    }
  }
  return IRLAP_NEGOTIATION_PARAM_UNSET;
}

static void normalize_params(irlap_negotiation_params_t* params) {
  int i;
  for(i = 0; i < IRLAP_NEGOTIATION_NUM_PARAMS; i++) {
    params->params[i] = get_most_significant_set_bit(params->params[i]);
  }
}

int irlap_negotiation_merge_params(irlap_negotiation_params_t* a, irlap_negotiation_params_t* b) {
  int i;
  for(i = 0; i < IRLAP_NEGOTIATION_NUM_PARAMS; i++) {
    struct irlap_negotiation_param* param = get_param_by_index(i);
    if(!param) {
      continue;
    }
    if(a->params[i] == IRLAP_NEGOTIATION_PARAM_UNSET) {
      return -EINVAL;
    }
    if(b->params[i] == IRLAP_NEGOTIATION_PARAM_UNSET) {
      return -EINVAL;
    }
    if(!IRLAP_NEGOTIATION_PARAM_IS_INDEPENDENT(param)) {
      irlap_negotiation_param_t res = a->params[i] & b->params[i];
      a->params[i] = res;
      b->params[i] = res;
      if(res == IRLAP_NEGOTIATION_PARAM_UNSET) {
        return -IRLAP_ERR_NO_COMMON_PARAMETERS_FOUND;
      }      
    }
  }
  normalize_params(a);
  normalize_params(b);
  return 0;
}

static inline uint32_t baudrate_bits_to_value(irlap_negotiation_param_t bits) {
  switch(bits) {
    case IRLAP_NEGOTIATION_BAUDRATE_2400:
      return 2400;
    case IRLAP_NEGOTIATION_BAUDRATE_9600:
      return 9600;
    case IRLAP_NEGOTIATION_BAUDRATE_19200:
      return 19200;
    case IRLAP_NEGOTIATION_BAUDRATE_38400:
      return 38400;
    case IRLAP_NEGOTIATION_BAUDRATE_57600:
      return 57600;
    case IRLAP_NEGOTIATION_BAUDRATE_115200:
      return 115200;
    case IRLAP_NEGOTIATION_BAUDRATE_576000:
      return 576000;
    case IRLAP_NEGOTIATION_BAUDRATE_1152000:
      return 1152000;
    case IRLAP_NEGOTIATION_BAUDRATE_4000000:
      return 4000000;
  }
  return 0;
}

static inline uint16_t max_turn_around_time_bits_to_value(irlap_negotiation_param_t bits) {
  switch(bits) {
    case IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_500_MS:
      return 500;
    case IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_250_MS:
      return 250;
    case IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_100_MS:
      return 100;
    case IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_50_MS:
      return 50;
  }
  return 0;
}


static inline uint16_t data_size_bits_to_value(irlap_negotiation_param_t bits) {
  uint8_t mask = 0b1;
  uint16_t data_size = 64;
  while(mask > 0) {
    if(bits & mask) {
      return data_size;
    }
    data_size *= 2;
    mask <<= 1;
  }
  return 0;
}

static inline uint8_t window_size_bits_to_value(irlap_negotiation_param_t bits) {
  uint8_t mask = 0b1;
  uint8_t window_size = 1;
  while(mask > 0) {
    if(bits & mask) {
      return window_size;
    }
    window_size++;
    mask <<= 1;
  }
  return 0;
}

static inline uint8_t additional_bofs_bits_to_value(irlap_negotiation_param_t bits, uint32_t baudrate) {
  uint8_t raw_bofs;
  switch(bits) {
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_48:
      raw_bofs = 48;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_24:
      raw_bofs = 24;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_12:
      raw_bofs = 12;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_5:
      raw_bofs = 5;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_3:
      raw_bofs = 3;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_2:
      raw_bofs = 2;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_1:
      raw_bofs = 1;
    case IRLAP_NEGOTIATION_ADDITIONAL_BOFS_0:
      return 0;
    default:
      return 0xFF;
  }

  switch(baudrate) {
    case 2400:
      return raw_bofs / 48;
    case 9600:
      return raw_bofs / 12;
    case 19200:
      return raw_bofs / 6;
    case 38400:
      return raw_bofs / 3;
    case 576000:
      return raw_bofs / 2;
    case 115200:
      return raw_bofs / 1;
  }
  return 0;
}

static inline uint16_t min_turn_around_time_bits_to_value(irlap_negotiation_param_t bits) {
  switch(bits) {
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_10000_US:
      return 10000;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_5000_US:
      return 5000;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_1000_US:
      return 1000;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_500_US:
      return 500;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_100_US:
      return 100;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_50_US:
      return 50;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_10_US:
      return 10;
    case IRLAP_NEGOTIATION_MIN_TURN_AROUND_TIME_0_US:
      return 0;
  }
  return 0xFFFF;
}

static inline uint8_t disconnect_threshold_bits_to_value(irlap_negotiation_param_t bits) {
  switch(bits) {
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_3_S:
      return 3;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_8_S:
      return 8;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_12_S:
      return 12;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_16_S:
      return 16;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_20_S:
      return 20;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_25_S:
      return 25;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_30_S:
      return 30;
    case IRLAP_NEGOTIATION_DISCONNECT_THRESHOLD_TIME_40_S:
      return 40;
  }
  return 0;
}

static struct irlap_negotiation_base_line_capacity max_line_capacity_table_500ms[] = {
  {    9600,    400 },
  {   19200,    800 },
  {   38400,   1600 },
  {   57600,   2360 },
  {  115200,   4800 },
  {  576000,  28800 },
  { 1152000,  57600 },
  { 4000000, 200000 },
  {       0,      0 },
};

static uint32_t get_max_line_capacity(uint32_t baudrate, uint16_t max_turn_around_time_ms) {
  struct irlap_negotiation_base_line_capacity* entry = max_line_capacity_table_500ms;
  while(entry->baudrate != 0) {
    if(entry->baudrate == baudrate) {
      return entry->capacity * (uint32_t)max_turn_around_time_ms / 500UL;
    }
    entry++;
  }
  return 0;
}

static inline uint16_t get_requested_line_capacity(irlap_negotiation_values_t* values) {
  uint16_t turn_around_bytes = (uint16_t)((uint64_t)values->baudrate * 1000000ULL / (uint64_t)values->min_turn_around_time_us);
  return (uint16_t)values->window_size * (values->data_size + 6 + (uint16_t)values->additional_bofs) + turn_around_bytes;
}

static int fit_line_capacity(irlap_negotiation_values_t* values) {
  bool reduce_window_size = true;
  uint32_t max_line_capacity = get_max_line_capacity(values->baudrate, values->max_turn_around_time_ms);
  if(max_line_capacity == 0) {
    return -EINVAL;
  }
  while(get_requested_line_capacity(values) > max_line_capacity) {
    if(values->data_size <= 64 && values->window_size <= 1) {
      return -EINVAL;
    }
    if(reduce_window_size) {
      if(values->window_size > 1) {
        values->window_size--;
      }
    } else {
      if(values->data_size > 64) {
        values->data_size /= 2;
      }
    }
    reduce_window_size = !reduce_window_size;
  }
  return 0;
}

int irlap_negotiation_translate_params_to_values(irlap_negotiation_values_t* values, irlap_negotiation_params_t* params) {
  values->baudrate = baudrate_bits_to_value(params->baudrate);
  if(values->baudrate == 0) {
    return -EINVAL;
  }
  values->max_turn_around_time_ms = max_turn_around_time_bits_to_value(params->max_turn_around_time);
  if(values->max_turn_around_time_ms == 0) {
    return -EINVAL;
  }
  values->data_size = data_size_bits_to_value(params->data_size);
  if(values->data_size == 0) {
    return -EINVAL;
  }
  values->window_size = window_size_bits_to_value(params->window_size);
  if(values->window_size == 0) {
    return -EINVAL;
  }
  values->additional_bofs = additional_bofs_bits_to_value(params->additional_bofs, values->baudrate);
  if(values->additional_bofs == 0xFF) {
    return -EINVAL;
  }
  values->min_turn_around_time_us = min_turn_around_time_bits_to_value(params->min_turn_around_time);
  if(values->min_turn_around_time_us == 0xFFFF) {
    return -EINVAL;
  }
  values->disconnect_threshold_time_s = disconnect_threshold_bits_to_value(params->disconnect_threshold);
  if(values->disconnect_threshold_time_s == 0) {
    return -EINVAL;
  }

  if(values->baudrate < 115200 && values->max_turn_around_time_ms != 500) {
    return -EINVAL;
  }

  return fit_line_capacity(values);
}

void irlap_negotiation_load_default_params(irlap_negotiation_params_t* params) {
  params->baudrate             = IRLAP_NEGOTIATION_BAUDRATE_9600;
  params->max_turn_around_time = IRLAP_NEGOTIATION_MAX_TURN_AROUND_TIME_500_MS;
  params->data_size            = IRLAP_NEGOTIATION_DATA_SIZE_64;
  params->window_size          = IRLAP_NEGOTIATION_WINDOW_SIZE_1;
  params->additional_bofs      = IRLAP_NEGOTIATION_ADDITIONAL_BOFS_0;
  params->min_turn_around_time = 0xFF;
}

void irlap_negotiation_load_default_values(irlap_negotiation_values_t* values) {
  irlap_negotiation_params_t params;
  irlap_negotiation_load_default_params(&params);
  normalize_params(&params);
  irlap_negotiation_translate_params_to_values(values, &params);
}
