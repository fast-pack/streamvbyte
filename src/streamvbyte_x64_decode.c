#include "streamvbyte_isadetection.h"
#include "streamvbyte_shuffle_tables_decode.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcast-align"
#endif

#ifdef STREAMVBYTE_X64
STREAMVBYTE_TARGET_SSE41
static inline __m128i svb_decode_sse41(uint32_t key,
                                  const uint8_t *__restrict__ *dataPtrPtr) {
  uint8_t len;
  __m128i Data = _mm_loadu_si128((const __m128i *)*dataPtrPtr);
  uint8_t *pshuf = (uint8_t *) &shuffleTable[key];
  __m128i Shuf = *(__m128i *)pshuf;
#ifdef AVOIDLENGTHLOOKUP
  // this avoids the dependency on lengthTable,
  // see https://github.com/lemire/streamvbyte/issues/12
  len = pshuf[12 + (key >> 6)] + 1;
#else
  len = lengthTable[key];
#endif
  Data = _mm_shuffle_epi8(Data, Shuf);
  *dataPtrPtr += len;
  return Data;
}
STREAMVBYTE_UNTARGET_REGION


STREAMVBYTE_TARGET_SSE41
static inline void svb_write_sse41(uint32_t *out, __m128i Vec) {
  _mm_storeu_si128((__m128i *)out, Vec);
}
STREAMVBYTE_UNTARGET_REGION



STREAMVBYTE_TARGET_SSE41
static inline const uint8_t *svb_decode_sse41_simple(uint32_t *out,
                                     const uint8_t *__restrict__ keyPtr,
                                     const uint8_t *__restrict__ dataPtr,
                                     uint64_t count) {

  uint64_t keybytes = count / 4; // number of key bytes
  __m128i Data;
  if (keybytes >= 8) {

    int64_t Offset = -(int64_t)keybytes / 8 + 1;

    const uint64_t *keyPtr64 = (const uint64_t *)keyPtr - Offset;
    uint64_t keys;
    for (; Offset != 1; ++Offset) {
      memcpy(&keys, keyPtr64 + Offset, sizeof(keys));

      Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
      svb_write_sse41(out, Data);
      Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
      svb_write_sse41(out + 4, Data);

      keys >>= 16;
      Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
      svb_write_sse41(out + 8, Data);
      Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
      svb_write_sse41(out + 12, Data);

      keys >>= 16;
      Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
      svb_write_sse41(out + 16, Data);
      Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
      svb_write_sse41(out + 20, Data);

      keys >>= 16;
      Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
      svb_write_sse41(out + 24, Data);
      Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
      svb_write_sse41(out + 28, Data);

      out += 32;
    }
  }
  return dataPtr;
}
STREAMVBYTE_UNTARGET_REGION

// Expands 32 1-byte integers (8 key bytes equal to 0x00). Each group of four
// bytes is loaded separately so that compilers can use pmovzxbd from memory
// instead of extra shifts (GCC is much slower with 16-byte loads and shifts).
STREAMVBYTE_TARGET_SSE41
static inline void svb_decode_32x1_sse41(const uint8_t *dataPtr, uint32_t *out) {
  for (int i = 0; i < 8; i++) {
    uint32_t four;
    memcpy(&four, dataPtr + 4 * i, sizeof(four));
    svb_write_sse41(out + 4 * i, _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)four)));
  }
}
STREAMVBYTE_UNTARGET_REGION

// Copies 32 4-byte integers (8 key bytes equal to 0xFF).
STREAMVBYTE_TARGET_SSE41
static inline void svb_copy_32x4_sse41(const uint8_t *dataPtr, uint32_t *out) {
  for (int i = 0; i < 8; i++)
    svb_write_sse41(out + 4 * i,
                    _mm_loadu_si128((const __m128i *)(dataPtr + 16 * i)));
}
STREAMVBYTE_UNTARGET_REGION

// Same as svb_decode_sse41_simple, but blocks of 32 integers that are all
// 1-byte or all 4-byte take dedicated fast paths. Only used by the *_branchy
// functions: the extra branches mispredict when only some blocks qualify.
STREAMVBYTE_TARGET_SSE41
static inline const uint8_t *
svb_decode_sse41_branchy(uint32_t *out, const uint8_t *__restrict__ keyPtr,
                         const uint8_t *__restrict__ dataPtr, uint64_t count) {
  uint64_t keybytes = count / 4; // number of key bytes
  __m128i Data;
  for (; keybytes >= 8; keybytes -= 8, keyPtr += 8, out += 32) {
    uint64_t keys;
    memcpy(&keys, keyPtr, sizeof(keys));
    if (keys == 0) {
      svb_decode_32x1_sse41(dataPtr, out);
      dataPtr += 32;
      continue;
    }
    if (keys == ~UINT64_C(0)) {
      svb_copy_32x4_sse41(dataPtr, out);
      dataPtr += 128;
      continue;
    }
    Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
    svb_write_sse41(out, Data);
    Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
    svb_write_sse41(out + 4, Data);

    keys >>= 16;
    Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
    svb_write_sse41(out + 8, Data);
    Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
    svb_write_sse41(out + 12, Data);

    keys >>= 16;
    Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
    svb_write_sse41(out + 16, Data);
    Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
    svb_write_sse41(out + 20, Data);

    keys >>= 16;
    Data = svb_decode_sse41((keys & 0xFF), &dataPtr);
    svb_write_sse41(out + 24, Data);
    Data = svb_decode_sse41((keys & 0xFF00) >> 8, &dataPtr);
    svb_write_sse41(out + 28, Data);
  }
  return dataPtr;
}
STREAMVBYTE_UNTARGET_REGION
#endif
