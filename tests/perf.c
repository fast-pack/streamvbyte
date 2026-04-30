#include "streamvbyte.h"
#include "counters/bench.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#endif

#define N 500000U

typedef struct {
  const uint32_t *datain;
  uint8_t *compressedbuffer;
  uint32_t *recovdata;
  size_t compsize;
  size_t n;
} svb_ctx;

static void bench_encode(void *ud) {
  svb_ctx *ctx = (svb_ctx *)ud;
  ctx->compsize = streamvbyte_encode(ctx->datain, (uint32_t)ctx->n, ctx->compressedbuffer);
}

static void bench_decode(void *ud) {
  svb_ctx *ctx = (svb_ctx *)ud;
  size_t r = streamvbyte_decode(ctx->compressedbuffer, ctx->recovdata, (uint32_t)ctx->n);
  (void)r;
}

static void print_aggregate(const char *label, size_t bytes_processed,
                            const counters_event_aggregate *agg, int have_counters) {
  double ns = counters_event_aggregate_elapsed_ns(agg);
  double gbs = ns > 0.0 ? (double)bytes_processed / ns : 0.0;
  double uints_per_sec = ns > 0.0 ? (double)(bytes_processed / sizeof(uint32_t)) * 1e9 / ns : 0.0;

  printf("  %-7s %8.0f ns/op  %6.2f GB/s  %12.0f uints/s  iters=%d",
         label, ns, gbs, uints_per_sec,
         counters_event_aggregate_iteration_count(agg));

  if (have_counters) {
    double instr = counters_event_aggregate_instructions(agg);
    double cycles = counters_event_aggregate_cycles(agg);
    double branches = counters_event_aggregate_branches(agg);
    double branch_misses = counters_event_aggregate_branch_misses(agg);
    double cache_misses = counters_event_aggregate_cache_misses(agg);
    double ipc = cycles > 0.0 ? instr / cycles : 0.0;
    size_t nints = bytes_processed / sizeof(uint32_t);
    double ins_per_int = nints > 0 ? instr / (double)nints : 0.0;
    double cyc_per_int = nints > 0 ? cycles / (double)nints : 0.0;
    printf("  ins=%.0f  cyc=%.0f  IPC=%.2f  ins/int=%.2f  cyc/int=%.2f"
           "  br=%.0f  brmiss=%.0f  cmiss=%.0f",
           instr, cycles, ipc, ins_per_int, cyc_per_int,
           branches, branch_misses, cache_misses);
  }
  printf("\n");
}

static void fill_log_uniform(uint32_t *data, size_t n) {
  for (size_t k = 0; k < n; ++k)
    data[k] = (uint32_t)rand() >> ((uint32_t)31 & (uint32_t)rand());
}

static void fill_full_range(uint32_t *data, size_t n) {
  for (size_t k = 0; k < n; ++k) {
    /* rand() yields at most 31 bits on most platforms; combine two calls. */
    uint32_t hi = (uint32_t)rand();
    uint32_t lo = (uint32_t)rand();
    data[k] = (hi << 16) ^ lo;
  }
}

static void fill_small(uint32_t *data, size_t n) {
  for (size_t k = 0; k < n; ++k)
    data[k] = (uint32_t)rand() & 0xFFu;
}

static void run_benchmark(const char *title,
                          void (*fill)(uint32_t *, size_t),
                          uint32_t *datain, uint8_t *compressedbuffer,
                          uint32_t *recovdata, int have_counters) {
  fill(datain, N);

  svb_ctx ctx;
  ctx.datain = datain;
  ctx.compressedbuffer = compressedbuffer;
  ctx.recovdata = recovdata;
  ctx.compsize = 0;
  ctx.n = N;

  counters_bench_parameter params = counters_bench_parameter_default();

  printf("== %s ==\n", title);

  counters_event_aggregate enc = counters_bench(bench_encode, &ctx, &params);
  print_aggregate("encode", N * sizeof(uint32_t), &enc, have_counters);

  size_t compsize = ctx.compsize;

  counters_event_aggregate dec = counters_bench(bench_decode, &ctx, &params);
  print_aggregate("decode", N * sizeof(uint32_t), &dec, have_counters);

  size_t compsize2 = streamvbyte_decode(compressedbuffer, recovdata, N);
  if (compsize != compsize2)
    printf("  compsize mismatch: %zu vs %zu\n", compsize, compsize2);

  uint32_t k;
  for (k = 0; k < N && datain[k] == recovdata[k]; k++)
    ;
  if (k < N)
    printf("  mismatch at %u before=%u after=%u\n", k, datain[k], recovdata[k]);
  assert(k >= N);

  double ratio = (double)compsize / (double)((size_t)N * sizeof(uint32_t));
  printf("  Compressed %zu bytes down to %zu bytes (ratio = %.3f, %.2f bits/int).\n\n",
         (size_t)N * sizeof(uint32_t), compsize, ratio,
         8.0 * (double)compsize / (double)N);
}

int main(void) {
  uint32_t *datain = (uint32_t *)malloc(N * sizeof(uint32_t));
  uint8_t *compressedbuffer = (uint8_t *)malloc((size_t)N * 5);
  uint32_t *recovdata = (uint32_t *)malloc(N * sizeof(uint32_t));
  if (!datain || !compressedbuffer || !recovdata) {
    fprintf(stderr, "allocation failure\n");
    return EXIT_FAILURE;
  }

  int have_counters = counters_has_performance_counters() ? 1 : 0;
  if (!have_counters) {
    fprintf(stderr,
        "Warning: hardware performance counters are unavailable.\n"
        "Timing will be reported, but instruction/cycle/branch/cache-miss\n"
        "counts will be omitted. To enable detailed counters:\n"
        "  Linux: relax perf_event_paranoid, e.g.\n"
        "    sudo sysctl -w kernel.perf_event_paranoid=0\n"
        "    (or run this binary with sudo)\n"
        "  macOS (Apple Silicon): run this binary with sudo to access kpc.\n\n");
  }

  run_benchmark("log-uniform widths (mixed 1-4 byte)", fill_log_uniform,
                datain, compressedbuffer, recovdata, have_counters);
  run_benchmark("full-range uint32_t (mostly 4-byte)", fill_full_range,
                datain, compressedbuffer, recovdata, have_counters);
  run_benchmark("small values [0,256) (1-byte)", fill_small,
                datain, compressedbuffer, recovdata, have_counters);

  free(datain);
  free(compressedbuffer);
  free(recovdata);
  return EXIT_SUCCESS;
}
