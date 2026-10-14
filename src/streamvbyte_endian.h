#ifndef STREAMVBYTE_ENDIAN_H
#define STREAMVBYTE_ENDIAN_H

#include <stddef.h>
#include <stdint.h>
#include <string.h> // for memcpy

// The stream vbyte format stores values in little-endian byte order.
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) &&               \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define STREAMVBYTE_BIG_ENDIAN 1
#endif

// Reads the 'len' (1 to 4) low-order bytes of a little-endian value.
static inline uint32_t svb_load_le(const uint8_t *p, size_t len) {
  uint32_t val = 0;
#ifdef STREAMVBYTE_BIG_ENDIAN
  for (size_t i = 0; i < len; i++) {
    val |= (uint32_t)p[i] << (8 * i);
  }
#else
  memcpy(&val, p, len);
#endif
  return val;
}

// Writes the 'len' (1 to 4) low-order bytes of 'val' in little-endian order.
static inline void svb_store_le(uint8_t *p, uint32_t val, size_t len) {
#ifdef STREAMVBYTE_BIG_ENDIAN
  for (size_t i = 0; i < len; i++) {
    p[i] = (uint8_t)(val >> (8 * i));
  }
#else
  memcpy(p, &val, len);
#endif
}

#endif // STREAMVBYTE_ENDIAN_H
