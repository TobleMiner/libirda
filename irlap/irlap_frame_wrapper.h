#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "irlap.h"

typedef enum {
  IRLAP_FRAME_WRAPPER_ASYNC
} irlap_frame_wrapper_t;

#define IRLAP_FRAME_WRAP_ASYNC_CE  0x7D
#define IRLAP_FRAME_WRAP_ASYNC_XOR 0x20

#define IRLAP_FRAME_WRAP_ASYNC_BOF            0xC0
#define IRLAP_FRAME_WRAP_ASYNC_BOF_ADDITIONAL 0xFF
#define IRLAP_FRAME_WRAP_ASYNC_EOF            0xC1

ssize_t irlap_wrapper_get_wrapped_size(irlap_frame_wrapper_t wrapper, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof);
ssize_t irlap_wrapper_wrap(irlap_frame_wrapper_t wrapper, uint8_t* dst, size_t dst_len, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof);
