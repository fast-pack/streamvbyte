#include "streamvbyte.h"
#include "streamvbytedelta.h"
#include "streamvbyte_zigzag.h"
#include "streamvbyte_isadetection.h"

#if STREAMVBYTE_IS_RISCV != 1
#error "expected STREAMVBYTE_IS_RISCV to be enabled"
#endif

#if STREAMVBYTE_IS_RISCV64 != 1
#error "expected STREAMVBYTE_IS_RISCV64 to be enabled"
#endif

#ifdef STREAMVBYTE_X64
#error "forced RISC-V smoke build must not keep STREAMVBYTE_X64"
#endif

void riscv_compile_smoke(void) {
  uint32_t values[8] = {1, 2, 3, 255, 256, 65535, 65536, 123456789};
  uint32_t recovered[8] = {0};
  uint32_t zz[8] = {0};
  int32_t signed_values[8] = {0, -1, 1, -2, 2, -3, 3, -4};
  uint8_t buffer[128] = {0};
  size_t encoded = streamvbyte_encode(values, 8, buffer);
  (void)streamvbyte_decode(buffer, recovered, 8);
  (void)streamvbyte_validate_stream(buffer, encoded, 8);
  zigzag_encode(signed_values, zz, 8);
  zigzag_decode(zz, signed_values, 8);
  (void)streamvbyte_delta_encode(values, 8, buffer, 0);
  (void)streamvbyte_delta_decode(buffer, recovered, 8, 0);
}
