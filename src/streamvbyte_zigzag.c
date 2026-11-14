#include "streamvbyte_zigzag.h"

static inline
uint32_t svb_zigzag_encode_32 (uint32_t val) {
	return (val + val) ^ (uint32_t)((int32_t)val >> 31);
}

void zigzag_encode(const int32_t * in, uint32_t * out, size_t N) {
    for(size_t i = 0; i < N; i++)
      out[i] = svb_zigzag_encode_32((uint32_t)in[i]);
}

void zigzag_delta_encode(const int32_t * in, uint32_t * out, size_t N, int32_t prev) {
    for (size_t i = 0; i < N; i++) {
      // Subtract as uint32_t: the difference of two int32_t values may
      // overflow int32_t (undefined behavior), but wraps around as intended.
      out[i] = svb_zigzag_encode_32((uint32_t)in[i] - (uint32_t)prev);
      prev = in[i];
    }
}

static inline
int32_t svb_zigzag_decode_32 (uint32_t val) {
	return (val >> 1) ^ (0-(val & 1));
}


void zigzag_decode(const uint32_t * in, int32_t * out, size_t N) {
    for(size_t i = 0; i < N; i++)
      out[i] = svb_zigzag_decode_32(in[i]);
}


void zigzag_delta_decode(const uint32_t * in, int32_t * out, size_t N, int32_t prev) {
    // Accumulate as uint32_t: the sum may overflow int32_t (undefined
    // behavior), but wraps around as intended.
    uint32_t acc = (uint32_t)prev;
    for(size_t i = 0; i < N; i++) {
      acc += (uint32_t)svb_zigzag_decode_32(in[i]);
      out[i] = (int32_t)acc;
    }
}
