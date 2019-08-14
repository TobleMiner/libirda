#include <stdint.h>

#include "crc.h"

static uint16_t crc16_update(uint16_t crc, uint16_t poly, uint8_t* data_p, size_t length) {
  uint8_t i;
  uint16_t data;

  if (length == 0)
    return crc;

  do {
    for ( i= 0, data = (uint16_t)0xff & *data_p++; i < 8; i++, data >>= 1) {
      if ((crc & 0x0001) ^ (data & 0x0001)) {
        crc = (crc >> 1) ^ poly;
      } else {
        crc >>= 1;
      }
    }
  } while (--length);

  return crc;
}

static uint16_t crc16_final(uint16_t crc) {
  return ~crc;
}

uint16_t irda_crc_ccitt_init() {
  return 0xFFFF;
}

uint16_t irda_crc_ccitt_update(uint16_t crc, uint8_t* data, size_t len) {
  return crc16_update(crc, IRDA_CRC_POLY_CCITT, data, len);
}

uint16_t irda_crc_ccitt_final(uint16_t crc) {
  return crc16_final(crc);
}
