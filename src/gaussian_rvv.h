#pragma once

#include "types.h"
// gaussianBlurRVV_LMUL1
// Same algorithm using LMUL=1 for the accumulator (spec section 6.2).
// Completes the required LMUL=1, 2, 4 sweep.
Image gaussianBlurRVV_LMUL1(const Image& input);
// RVV-optimized Gaussian blur.
//it have the same  output meaning as gaussianBlur(),
// but uses RISC-V Vector intrinsics for the interior pixels.
Image gaussianBlurRVV(const Image& input);
// gaussianBlurRVV_LMUL2
// Same algorithm but using LMUL=2 instead of LMUL=4 for the accumulator.
// Spec section 6.2 asks us to experiment with LMUL=1, 2, 4 and measure
// the tradeoff between elements-per-operation and register pressure.
Image gaussianBlurRVV_LMUL2(const Image& input);
