#pragma once

#include <stdint.h>
#include <sys/types.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define container_of(ptr, type, member) ({      \
  const typeof(((type *)0)->member) * __mptr = (ptr); \
  (type *)((char *)__mptr - offsetof(type, member)); })

#define CLAMP(x, h, l) (max(min((x), (l)), (h)))

#define ARRAY_LEN(arr) (sizeof((arr)) / sizeof(*(arr)))

#define BOOL_TO_STR(x) ((x) ? "true" : "false")

void hexdump(uint8_t* data, size_t len);
