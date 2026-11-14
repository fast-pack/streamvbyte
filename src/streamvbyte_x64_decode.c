#include "streamvbyte_isadetection.h"
#include "streamvbyte_shuffle_tables_decode.h"
#include <string.h> // for memcpy

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcast-align"
#endif

#ifdef STREAMVBYTE_X64
STREAMVBYTE_TARGET_SSE41
static inline __m128i
svb_decode_sse41(uint32_t key, const uint8_t *__restrict__ *dataPtrPtr) {
  uint8_t len;
  __m128i Data = _mm_loadu_si128((const __m128i *)*dataPtrPtr);
  uint8_t *pshuf = (uint8_t *)&shuffleTable[key];
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
static inline const uint8_t *
svb_decode_sse41_simple(uint32_t *out, const uint8_t *__restrict__ keyPtr,
                        const uint8_t *__restrict__ dataPtr, uint64_t count) {

  uint64_t keybytes = count / 4; // number of key bytes
  __m128i Data;
  if (keybytes >= 8) {

    int64_t Offset = -(int64_t)keybytes / 8 + 1;

    const uint64_t *keyPtr64 = (const uint64_t *)keyPtr - Offset;
    uint64_t keys;
    for (; Offset != 1; ++Offset) {
      memcpy(&keys, keyPtr64 + Offset, sizeof(keys));

      if (keys == 0) {
        uint32_t d0, d1, d2, d3, d4, d5, d6, d7;
        memcpy(&d0, dataPtr + 0, 4);
        memcpy(&d1, dataPtr + 4, 4);
        memcpy(&d2, dataPtr + 8, 4);
        memcpy(&d3, dataPtr + 12, 4);
        memcpy(&d4, dataPtr + 16, 4);
        memcpy(&d5, dataPtr + 20, 4);
        memcpy(&d6, dataPtr + 24, 4);
        memcpy(&d7, dataPtr + 28, 4);
        svb_write_sse41(out + 0, _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d0)));
        svb_write_sse41(out + 4, _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d1)));
        svb_write_sse41(out + 8, _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d2)));
        svb_write_sse41(out + 12,
                        _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d3)));
        svb_write_sse41(out + 16,
                        _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d4)));
        svb_write_sse41(out + 20,
                        _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d5)));
        svb_write_sse41(out + 24,
                        _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d6)));
        svb_write_sse41(out + 28,
                        _mm_cvtepu8_epi32(_mm_cvtsi32_si128((int)d7)));
        dataPtr += 32;
        out += 32;
        continue;
      }
      if (keys == ~0ULL) {
        __m128i c0 = _mm_loadu_si128((const __m128i *)(dataPtr + 0));
        __m128i c1 = _mm_loadu_si128((const __m128i *)(dataPtr + 16));
        __m128i c2 = _mm_loadu_si128((const __m128i *)(dataPtr + 32));
        __m128i c3 = _mm_loadu_si128((const __m128i *)(dataPtr + 48));
        __m128i c4 = _mm_loadu_si128((const __m128i *)(dataPtr + 64));
        __m128i c5 = _mm_loadu_si128((const __m128i *)(dataPtr + 80));
        __m128i c6 = _mm_loadu_si128((const __m128i *)(dataPtr + 96));
        __m128i c7 = _mm_loadu_si128((const __m128i *)(dataPtr + 112));
        svb_write_sse41(out + 0, c0);
        svb_write_sse41(out + 4, c1);
        svb_write_sse41(out + 8, c2);
        svb_write_sse41(out + 12, c3);
        svb_write_sse41(out + 16, c4);
        svb_write_sse41(out + 20, c5);
        svb_write_sse41(out + 24, c6);
        svb_write_sse41(out + 28, c7);
        dataPtr += 128;
        out += 32;
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

      out += 32;
    }
  }
  return dataPtr;
}
STREAMVBYTE_UNTARGET_REGION
#endif
