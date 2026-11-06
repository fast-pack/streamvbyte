#include "streamvbyte_isadetection.h"

#if defined(STREAMVBYTE_IS_ARM64)

static inline uint32x4_t neon_delta(uint32x4_t curr, uint32x4_t prev) {
  return vsubq_u32(curr, vextq_u32(prev, curr, 3));
}

size_t streamvbyte_encode_neon_d1_init(const uint32_t *in, uint32_t count,
                                       uint8_t *out, uint32_t prev) {
  uint32_t keyLen = (count + 3) / 4;
  uint8_t *__restrict__ keyPtr = out;
  uint8_t *__restrict__ dataPtr = out + keyLen;

  uint32x4_t Prev = vdupq_n_u32(prev);

  const uint32_t *end = in + (count & ~7U);
  while (in != end) {
    uint32x4_t raw0 = vld1q_u32(in);
    uint32x4_t d0 = neon_delta(raw0, Prev);
    Prev = raw0;

    uint32x4_t raw1 = vld1q_u32(in + 4);
    uint32x4_t d1 = neon_delta(raw1, Prev);
    Prev = raw1;

    dataPtr += streamvbyte_encode4(d0, dataPtr, keyPtr);
    dataPtr += streamvbyte_encode4(d1, dataPtr, keyPtr + 1);

    keyPtr += 2;
    in += 8;
  }

  // Extract last element for scalar tail
  prev = vgetq_lane_u32(Prev, 3);
  return (
      size_t)(svb_encode_scalar_d1_init(in, keyPtr, dataPtr, count & 7, prev) -
              out);
}

#endif
