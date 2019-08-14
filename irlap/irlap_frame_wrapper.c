#include <errno.h>

#include "../util/crc.h"

#include "irlap_frame_wrapper.h"

static size_t irlap_wrapper_get_wrapped_size_async_(uint8_t* data, size_t len) {
  size_t wrapped_size = len;
  while(len--) {
    if(*data == IRLAP_FRAME_WRAP_ASYNC_CE  ||
       *data == IRLAP_FRAME_WRAP_ASYNC_BOF ||
       *data == IRLAP_FRAME_WRAP_ASYNC_EOF) {
      wrapped_size++;
    }
    data++;
  }
  return wrapped_size;
}

static size_t irlap_wrapper_get_wrapped_size_async(irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof) {
  union {
    uint16_t crc;
    uint8_t data[2];
  } crc;
  size_t wrapped_size;
  crc.crc = irda_crc_ccitt_init();
  crc.crc = irda_crc_ccitt_update(crc.crc,hdr->data, sizeof(hdr->data));
  crc.crc = irda_crc_ccitt_update(crc.crc, data, len);
  crc.crc = irda_crc_ccitt_final(crc.crc);
  // Additional bofs
  wrapped_size = num_additional_bof;
  // Actual bof
  wrapped_size += 1;
  // Size of header
  wrapped_size += irlap_wrapper_get_wrapped_size_async_(hdr->data, sizeof(hdr->data));
  // Size of payload
  wrapped_size += irlap_wrapper_get_wrapped_size_async_(data, len);
  // Size of crc
  wrapped_size += irlap_wrapper_get_wrapped_size_async_(crc.data, sizeof(crc.data));
  // End of frame
  wrapped_size += 1;
  return wrapped_size;
}

ssize_t irlap_wrapper_get_wrapped_size(irlap_frame_wrapper_t wrapper, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof) {
  switch(wrapper) {
    case IRLAP_FRAME_WRAPPER_ASYNC:
      return irlap_wrapper_get_wrapped_size_async(hdr, data, len, num_additional_bof);
  }
  return -EINVAL;
}

static size_t irlap_wrapper_wrap_async_data(uint8_t* dst, uint8_t* data, size_t len, uint8_t** next_dst) {
  size_t wrapped_size = 0;
  while(len-- > 0) {
    if(*data == IRLAP_FRAME_WRAP_ASYNC_CE  ||
       *data == IRLAP_FRAME_WRAP_ASYNC_BOF ||
       *data == IRLAP_FRAME_WRAP_ASYNC_EOF) {
      *dst++ = IRLAP_FRAME_WRAP_ASYNC_CE;
      wrapped_size++;
      *dst++ = *data++ ^ IRLAP_FRAME_WRAP_ASYNC_XOR;
      wrapped_size++;
    } else {
      *dst++ = *data++;
      wrapped_size++;
    }
  }
  if(next_dst != NULL) {
    *next_dst = dst;
  }
  return wrapped_size;
}

static size_t irlap_wrapper_wrap_async(uint8_t* dst, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof) {
  union {
    uint16_t crc;
    uint8_t data[2];
  } crc;
  size_t wrapped_size = 0;
  // Additional BOFs
  while(num_additional_bof-- > 0) {
    *dst++ = IRLAP_FRAME_WRAP_ASYNC_BOF_ADDITIONAL;
    wrapped_size++;
  }

  // Actual BOF
  *dst++ = IRLAP_FRAME_WRAP_ASYNC_BOF;
  wrapped_size++;

  // Header
  wrapped_size += irlap_wrapper_wrap_async_data(dst, hdr->data, sizeof(hdr->data), &dst);
  // Payload
  wrapped_size += irlap_wrapper_wrap_async_data(dst, data, len, &dst);

  // CRC
  crc.crc = irda_crc_ccitt_init();
  crc.crc = irda_crc_ccitt_update(crc.crc,hdr->data, sizeof(hdr->data));
  crc.crc = irda_crc_ccitt_update(crc.crc, data, len);
  crc.crc = irda_crc_ccitt_final(crc.crc);
  wrapped_size += irlap_wrapper_wrap_async_data(dst, crc.data, sizeof(crc.data), &dst);

  // EOF
  *dst++ = IRLAP_FRAME_WRAP_ASYNC_EOF;
  wrapped_size++;

  return wrapped_size;
}

ssize_t irlap_wrapper_wrap(irlap_frame_wrapper_t wrapper, uint8_t* dst, size_t dst_len, irlap_frame_hdr_t* hdr, uint8_t* data, size_t len, unsigned int num_additional_bof) {
  ssize_t required_len = irlap_wrapper_get_wrapped_size(wrapper, hdr, data, len, num_additional_bof);
  if(required_len < 0) {
    return required_len;
  }
  if(required_len > dst_len) {
    return -EINVAL;
  }
  switch(wrapper) {
    case IRLAP_FRAME_WRAPPER_ASYNC:
      return irlap_wrapper_wrap_async(dst, hdr, data, len, num_additional_bof);
  }
  return -EINVAL;
}
