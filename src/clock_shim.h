#pragma once
// clock_shim.h — pairs with clock_shim.cpp.
//
// Why this exists: bare-metal newlib's <time.h> doesn't declare
// CLOCK_MONOTONIC or clock_gettime() on riscv64-unknown-elf — there's no OS,
// so it doesn't promise these exist. clock_shim.cpp DEFINES a real
// clock_gettime at link time, but the compiler still needs to see its
// DECLARATION when it compiles any file that calls it. This header provides
// that declaration.
//
// Usage: in any RV-only .cpp file that calls clock_gettime/CLOCK_MONOTONIC
// (main.cpp, benchmark*.cpp, etc.), add this include alongside <time.h>:
//
//   #include <time.h>
#include "clock_shim.h"
//   #include "clock_shim.h"
//
// Keep <time.h> too — you still need `struct timespec` from it.

#include <time.h>
#include "clock_shim.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

extern "C" int clock_gettime(clockid_t clk_id, struct timespec* tp);

