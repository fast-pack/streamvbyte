

#include "streamvbyte_isadetection.h"
#include <string.h> // for memcpy
#ifdef STREAMVBYTE_ARM
#include "streamvbyte_shuffle_tables_decode.h"
#ifdef STREAMVBYTE_IS_ARM64
typedef uint8x16_t decode_t;
#else
typedef uint8x8x2_t decode_t;
#endif
static inline decode_t _decode_neon(const uint8_t key,
                                    const uint8_t *__restrict__ *dataPtrPtr) {

  uint8_t len;
  const uint8_t *pshuf = (const uint8_t *)&shuffleTable[key];
  uint8x16_t decodingShuffle = vld1q_u8(pshuf);

  uint8x16_t compressed = vld1q_u8(*dataPtrPtr);
#ifdef AVOIDLENGTHLOOKUP
  // this avoids the dependency on lengthTable,
  // see https://github.com/lemire/streamvbyte/issues/12
  len = pshuf[12 + (key >> 6)] + 1;
#else
  len = lengthTable[key];
#endif
#ifdef STREAMVBYTE_IS_ARM64
  uint8x16_t data = vqtbl1q_u8(compressed, decodingShuffle);
#else
  uint8x8x2_t codehalves = {
      {vget_low_u8(compressed), vget_high_u8(compressed)}};

  uint8x8x2_t data = {{vtbl2_u8(codehalves, vget_low_u8(decodingShuffle)),
                       vtbl2_u8(codehalves, vget_high_u8(decodingShuffle))}};
#endif
  *dataPtrPtr += len;
  return data;
}

static void streamvbyte_decode_quad(const uint8_t *__restrict__ *dataPtrPtr,
                                    uint8_t key, uint32_t *__restrict__ out) {
  decode_t data = _decode_neon(key, dataPtrPtr);
#ifdef STREAMVBYTE_IS_ARM64
  vst1q_u8((uint8_t *)out, data);
#else
  vst1_u8((uint8_t *)out, data.val[0]);
  vst1_u8((uint8_t *)(out + 2), data.val[1]);
#endif
}

static const uint8_t *svb_decode_vector(uint32_t *__restrict__ out,
                                        const uint8_t *__restrict__ keyPtr,
                                        const uint8_t *__restrict__ dataPtr,
                                        uint32_t count) {
  uint32_t keybytes = count / 4;
  while (keybytes >= 8) {
    uint64_t keys;
    memcpy(&keys, keyPtr, sizeof(keys));

    if (keys == 0) {
      uint8x16_t v0 = vld1q_u8(dataPtr);
      uint8x16_t v1 = vld1q_u8(dataPtr + 16);
      uint16x8_t lo0 = vmovl_u8(vget_low_u8(v0));
      uint16x8_t hi0 = vmovl_u8(vget_high_u8(v0));
      uint16x8_t lo1 = vmovl_u8(vget_low_u8(v1));
      uint16x8_t hi1 = vmovl_u8(vget_high_u8(v1));
      vst1q_u32(out + 0, vmovl_u16(vget_low_u16(lo0)));
      vst1q_u32(out + 4, vmovl_u16(vget_high_u16(lo0)));
      vst1q_u32(out + 8, vmovl_u16(vget_low_u16(hi0)));
      vst1q_u32(out + 12, vmovl_u16(vget_high_u16(hi0)));
      vst1q_u32(out + 16, vmovl_u16(vget_low_u16(lo1)));
      vst1q_u32(out + 20, vmovl_u16(vget_high_u16(lo1)));
      vst1q_u32(out + 24, vmovl_u16(vget_low_u16(hi1)));
      vst1q_u32(out + 28, vmovl_u16(vget_high_u16(hi1)));
      dataPtr += 32;
      out += 32;
      keyPtr += 8;
      keybytes -= 8;
      continue;
    }
    if (keys == ~0ULL) {
      uint8x16_t c0 = vld1q_u8(dataPtr + 0);
      uint8x16_t c1 = vld1q_u8(dataPtr + 16);
      uint8x16_t c2 = vld1q_u8(dataPtr + 32);
      uint8x16_t c3 = vld1q_u8(dataPtr + 48);
      uint8x16_t c4 = vld1q_u8(dataPtr + 64);
      uint8x16_t c5 = vld1q_u8(dataPtr + 80);
      uint8x16_t c6 = vld1q_u8(dataPtr + 96);
      uint8x16_t c7 = vld1q_u8(dataPtr + 112);
      vst1q_u8((uint8_t *)(out + 0), c0);
      vst1q_u8((uint8_t *)(out + 4), c1);
      vst1q_u8((uint8_t *)(out + 8), c2);
      vst1q_u8((uint8_t *)(out + 12), c3);
      vst1q_u8((uint8_t *)(out + 16), c4);
      vst1q_u8((uint8_t *)(out + 20), c5);
      vst1q_u8((uint8_t *)(out + 24), c6);
      vst1q_u8((uint8_t *)(out + 28), c7);
      dataPtr += 128;
      out += 32;
      keyPtr += 8;
      keybytes -= 8;
      continue;
    }

#if defined(__clang__)
#pragma clang loop unroll(disable)
#elif defined(__GNUC__)
#pragma GCC unroll 1
#endif
    for (uint32_t i = 0; i < 8; i++) {
      streamvbyte_decode_quad(&dataPtr, (uint8_t)keys, out + 4 * i);
      keys >>= 8;
    }
    out += 32;
    keyPtr += 8;
    keybytes -= 8;
  }
  for (uint32_t i = 0; i < keybytes; i++)
    streamvbyte_decode_quad(&dataPtr, keyPtr[i], out + 4 * i);

  return dataPtr;
}

static uint64_t svb_validate_vector(const uint8_t **keyPtrPtr,
                                    uint32_t *countPtr) {
  // Reduce the count by how many we'll process
  const uint32_t count = *countPtr & ~7U;
  const uint8_t *keyPtr = *keyPtrPtr;
  *countPtr &= 7;
  *keyPtrPtr += count / 4;

  // Deal with each of the 4 keys in a separate lane
  static const int32_t shift_vals[4] = {0, -2, -4, -6};
  const int32x4_t shifts = vld1q_s32(shift_vals);
  const uint32x4_t mask = vdupq_n_u32(3);
  uint32x4_t acc0 = vdupq_n_u32(0);
  uint32x4_t acc1 = vdupq_n_u32(0);

  // Unrolling more than twice doesn't seem to improve performance
  for (uint32_t c = 0; c < count; c += 8) {
    uint32x4_t shifted0 = vshlq_u32(vdupq_n_u32(*keyPtr++), shifts);
    acc0 = vaddq_u32(acc0, vandq_u32(shifted0, mask));
    uint32x4_t shifted1 = vshlq_u32(vdupq_n_u32(*keyPtr++), shifts);
    acc1 = vaddq_u32(acc1, vandq_u32(shifted1, mask));
  }

  // Accumulate the sums and add the +1 for each element (count)
  uint64x2_t sum0 = vpaddlq_u32(acc0);
  uint64x2_t sum1 = vpaddlq_u32(acc1);
  return vgetq_lane_u64(sum0, 0) + vgetq_lane_u64(sum0, 1) +
         vgetq_lane_u64(sum1, 0) + vgetq_lane_u64(sum1, 1) + count;
}
#endif
