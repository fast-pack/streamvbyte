#include "streamvbyte_isadetection.h"
#include <string.h>

#if defined(STREAMVBYTE_IS_ARM64)

static inline uint32x4_t svb_write_neon_d1(uint32_t *out, uint32x4_t curr,
                                           uint32x4_t prev, uint32x4_t zero) {
  uint32x4_t add1 = vextq_u32(zero, curr, 3);
  uint32x4_t p_last = vdupq_laneq_u32(prev, 3);
  curr = vaddq_u32(curr, add1);
  uint32x4_t add2 = vextq_u32(zero, curr, 2);
  curr = vaddq_u32(curr, p_last);
  curr = vaddq_u32(curr, add2);
  vst1q_u32(out, curr);
  return curr;
}

const uint8_t *svb_decode_neon_d1_init(uint32_t *out,
                                       const uint8_t *__restrict__ keyPtr,
                                       const uint8_t *__restrict__ dataPtr,
                                       uint64_t count, uint32_t prev) {
  uint64_t keybytes = count / 4;
  if (keybytes >= 8) {
    uint32x4_t Prev = vdupq_n_u32(prev);
    const uint32x4_t zero = vdupq_n_u32(0);

    const uint8_t *kPtr = keyPtr;
    uint64_t nextkeys;
    memcpy(&nextkeys, kPtr, sizeof(nextkeys));
    kPtr += sizeof(nextkeys);

    for (size_t b = 0; b < (keybytes / 8) - 1; ++b) {
      uint64_t keys = nextkeys;
      memcpy(&nextkeys, kPtr, sizeof(nextkeys));
      kPtr += sizeof(nextkeys);

      for (int i = 0; i < 8; ++i) {
        uint8_t key = (uint8_t)(keys >> (8 * i));
        uint8x16_t data = _decode_neon(key, &dataPtr);
        Prev = svb_write_neon_d1(out + 4 * i, vreinterpretq_u32_u8(data), Prev,
                                 zero);
      }
      out += 32;
    }

    uint64_t keys = nextkeys;
    for (int i = 0; i < 8; ++i) {
      uint8_t key = (uint8_t)(keys >> (8 * i));
      uint8x16_t data = _decode_neon(key, &dataPtr);
      Prev = svb_write_neon_d1(out + 4 * i, vreinterpretq_u32_u8(data), Prev,
                               zero);
    }
    out += 32;

    prev = out[-1];
  }

  uint64_t consumedkeys = keybytes - (keybytes & 7);
  return svb_decode_scalar_d1_init(out, keyPtr + consumedkeys, dataPtr,
                                   count & 31, prev);
}

#endif
