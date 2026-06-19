// clock_shim.cpp — makes clock_gettime(CLOCK_MONOTONIC, ...) actually work
// on riscv64-unknown-elf, so none of your existing benchmark/main files
// need to change AT ALL. Just compile + link this one extra file in.
//
// Why this is needed: bare-metal newlib has no OS to ask for the time, so
// clock_gettime is either missing at link time or a no-op stub. This
// provides a real implementation by reading the same `time` CSR that QEMU
// backs with its virtual timer (commonly 10 MHz on QEMU's virt machine).
//
// Add clock_shim.cpp to your RV_CXX build (it must NOT be compiled into the
// host/test build — it overrides a libc function and only makes sense
// bare-metal). In the Makefile that means adding it to LIB_SRCS_RVV_ONLY
// (or a new RV-only list) so it only ever goes through RV_CXX.

#include <time.h>
#include "clock_shim.h"
#include <cstdint>
#include "clock_shim.h"
extern "C" int clock_gettime(clockid_t /*clk_id*/, struct timespec* tp) {
    uint64_t ticks;
    asm volatile("rdtime %0" : "=r"(ticks));

    // QEMU's virt machine timebase-frequency is commonly 10,000,000 Hz
    // (10 MHz). If your QEMU invocation overrides this, change TIMEBASE_HZ
    // to match — otherwise absolute ms numbers will be off (relative
    // comparisons like O0 vs O3 stay valid either way, since both runs
    // use the same wrong-or-right frequency consistently).
    constexpr uint64_t TIMEBASE_HZ = 10000000ULL;

    tp->tv_sec  = static_cast<time_t>(ticks / TIMEBASE_HZ);
    tp->tv_nsec = static_cast<long>((ticks % TIMEBASE_HZ) * (1000000000ULL / TIMEBASE_HZ));
    return 0;
}

