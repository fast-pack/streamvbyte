#include "streamvbyte_isadetection.h"
#include <string.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#pragma clang diagnostic ignored "-Wcast-align"
#endif

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

static inline uint32x4_t svb_write_16bit_neon_d1(uint32_t *out,
                                                 uint16x8_t vec16,
                                                 uint32x4_t prev,
                                                 uint16x8_t zero16) {
  uint16x8_t add1 = vextq_u16(zero16, vec16, 7);
  vec16 = vaddq_u16(vec16, add1);
  uint16x8_t add2 = vextq_u16(zero16, vec16, 6);
  vec16 = vaddq_u16(vec16, add2);

  uint32x4_t v1 = vaddw_u16(vdupq_laneq_u32(prev, 3), vget_low_u16(vec16));
  uint32x4_t v2 = vaddw_u16(v1, vget_high_u16(vec16));

  vst1q_u32(out, v1);
  vst1q_u32(out + 4, v2);
  return v2;
}

const uint8_t *svb_decode_neon_d1_init(uint32_t *out,
                                       const uint8_t *__restrict__ keyPtr,
                                       const uint8_t *__restrict__ dataPtr,
                                       uint64_t count, uint32_t prev) {
  uint64_t keybytes = count / 4;
  if (keybytes >= 8) {
    uint32x4_t Prev = vdupq_n_u32(prev);
    const uint32x4_t zero = vdupq_n_u32(0);
    const uint16x8_t zero16 = vdupq_n_u16(0);

    const uint8_t *kPtr = keyPtr;
    uint64_t nextkeys;
    memcpy(&nextkeys, kPtr, sizeof(nextkeys));
    kPtr += sizeof(nextkeys);

    for (size_t b = 0; b < (keybytes / 8) - 1; ++b) {
      uint64_t keys = nextkeys;
      memcpy(&nextkeys, kPtr, sizeof(nextkeys));
      kPtr += sizeof(nextkeys);

      if (!keys) {
        Prev = svb_write_16bit_neon_d1(out, vmovl_u8(vld1_u8(dataPtr)), Prev,
                                       zero16);
        Prev = svb_write_16bit_neon_d1(out + 8, vmovl_u8(vld1_u8(dataPtr + 8)),
                                       Prev, zero16);
        Prev = svb_write_16bit_neon_d1(
            out + 16, vmovl_u8(vld1_u8(dataPtr + 16)), Prev, zero16);
        Prev = svb_write_16bit_neon_d1(
            out + 24, vmovl_u8(vld1_u8(dataPtr + 24)), Prev, zero16);
        out += 32;
        dataPtr += 32;
        continue;
      }

      for (int i = 0; i < 8; ++i) {
        uint8_t key = (uint8_t)(keys >> (8 * i));
        uint8x16_t data = _decode_neon(key, &dataPtr);
        Prev = svb_write_neon_d1(out + 4 * i, vreinterpretq_u32_u8(data), Prev,
                                 zero);
      }
      out += 32;
    }

    uint64_t keys = nextkeys;
    if (!keys) {
      Prev = svb_write_16bit_neon_d1(out, vmovl_u8(vld1_u8(dataPtr)), Prev,
                                     zero16);
      Prev = svb_write_16bit_neon_d1(out + 8, vmovl_u8(vld1_u8(dataPtr + 8)),
                                     Prev, zero16);
      Prev = svb_write_16bit_neon_d1(out + 16, vmovl_u8(vld1_u8(dataPtr + 16)),
                                     Prev, zero16);
      Prev = svb_write_16bit_neon_d1(out + 24, vmovl_u8(vld1_u8(dataPtr + 24)),
                                     Prev, zero16);
      out += 32;
      dataPtr += 32;
    } else {
      for (int i = 0; i < 8; ++i) {
        uint8_t key = (uint8_t)(keys >> (8 * i));
        uint8x16_t data = _decode_neon(key, &dataPtr);
        Prev = svb_write_neon_d1(out + 4 * i, vreinterpretq_u32_u8(data), Prev,
                                 zero);
      }
      out += 32;
    }

    prev = out[-1];
  }

  uint64_t consumedkeys = keybytes - (keybytes & 7);
  return svb_decode_scalar_d1_init(out, keyPtr + consumedkeys, dataPtr,
                                   count & 31, prev);
}

#endif
