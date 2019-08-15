#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "irlap_defs.h"

typedef struct {
  bool in_frame;
  uint8_t prev_byte;
  uint8_t data[IRLAP_MAX_DATA_SIZE];
  off_t write_ptr;
} irlap_wrapper_state_t;

#include "irlap.h"

typedef enum {
  IRLAP_FRAME_WRAPPER_ASYNC
} irlap_frame_wrapper_t;

// Absoulte maximum number of bytes wrapping layer needs to parse to find a frame
#define IRLAP_FRAME_MAX_SIZE (IRLAP_MAX_DATA_SIZE * 2 + IRLAP_FRAME_ADDITIONAL_BOF_MAX + 1 + 2 + 1)

#define IRLAP_FRAME_WRAP_ASYNC_CE  0x7D
#define IRLAP_FRAME_WRAP_ASYNC_XOR 0x20

#define IRLAP_FRAME_WRAP_ASYNC_BOF            0xC0
#define IRLAP_FRAME_WRAP_ASYNC_BOF_ADDITIONAL 0xFF
#define IRLAP_FRAME_WRAP_ASYNC_EOF            0xC1

#define IRLAP_FRAME_IS_CE(c) ( \
  (c == IRLAP_FRAME_WRAP_ASYNC_CE) \
)

#define IRLAP_FRAME_IS_BOF(c) ( \
  (c == IRLAP_FRAME_WRAP_ASYNC_BOF) \
)

#define IRLAP_FRAME_IS_EOF(c) ( \
  (c == IRLAP_FRAME_WRAP_ASYNC_EOF) \
)

typedef int (*irlap_wrapper_handle_cb_f)(uint8_t* data, size_t len, void * priv);

ssize_t irlap_wrapper_get_wrapped_size(irlap_frame_wrapper_t wrapper, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof);
ssize_t irlap_wrapper_wrap(irlap_frame_wrapper_t wrapper, uint8_t* dst, size_t dst_len, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof);
int irlap_wrapper_unwrap(irlap_frame_wrapper_t wrapper, irlap_wrapper_state_t* state, uint8_t* data, size_t len, irlap_wrapper_handle_cb_f cb, void* priv);
