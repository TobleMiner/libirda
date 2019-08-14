#pragma once

#include <stdint.h>
#include <sys/types.h>

#define IRDA_CRC_POLY_CCITT 0x8408

uint16_t irda_crc_ccitt_init();
uint16_t irda_crc_ccitt_update(uint16_t crc, uint8_t* data, size_t len);
uint16_t irda_crc_ccitt_final(uint16_t crc);
