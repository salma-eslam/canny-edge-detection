#include "gaussian_rvv.h"
#include <algorithm>
#include <cstdint>
#include <riscv_vector.h>

static const uint16_t GAUSSIAN_KERNEL[5][5] = {
    { 1,  4,  7,  4,  1},
    { 4, 16, 26, 16,  4},
    { 7, 26, 41, 26,  7},
    { 4, 16, 26, 16,  4},
    { 1,  4,  7,  4,  1}
};

static const int R = 2;

static inline uint8_t normalizeApproxScalar(uint32_t sum) {
    uint32_t value = (sum * 240u) >> 16;
    return static_cast<uint8_t>(std::clamp<uint32_t>(value, 0, 255));
}

static uint8_t computeGaussianPixelScalarBorder(const Image& input, int x, int y) {
    int W = input.width;
    int H = input.height;
    uint32_t accumulator = 0;

    for (int ky = -R; ky <= R; ky++) {
        for (int kx = -R; kx <= R; kx++) {
            int sourceX = x + kx;
            int sourceY = y + ky;
            if (sourceX < 0 || sourceX >= W || sourceY < 0 || sourceY >= H) continue;

            uint8_t pixel = input.data[sourceY * W + sourceX];
            uint16_t coefficient = GAUSSIAN_KERNEL[ky + R][kx + R];
            accumulator += static_cast<uint32_t>(pixel) * static_cast<uint32_t>(coefficient);
        }
    }
    return normalizeApproxScalar(accumulator);
}

Image gaussianBlurRVV(const Image& input) {
    int width = input.width;
    int height = input.height;

    Image output;
    output.width = width;
    output.height = height;
    output.data.resize(width * height, 0);

    // 1. Border handling
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if ((x < R) || (x >= width - R) || (y < R) || (y >= height - R)) {
                output.data[y * width + x] = computeGaussianPixelScalarBorder(input, x, y);
            }
        }
    }

    // 2. Vectorized Interior Processing
    for (int y = R; y < height - R; y++) {
        int x = R;

        while (x < width - R) {
            size_t remainingPixels = static_cast<size_t>((width - R) - x);
            
            // CRITICAL FIX: Establish vector length ONCE out here
            size_t vl = __riscv_vsetvl_e8m1(remainingPixels);

            // Clear accumulator track
            vuint32m4_t accumulator = __riscv_vmv_v_x_u32m4(0, vl);

            for (int ky = -R; ky <= R; ky++) {
                // Pre-calculate the row baseline pointer offset
                const uint8_t* row_ptr = &input.data[(y + ky) * width + x];

                for (int kx = -R; kx <= R; kx++) {
                    uint16_t coefficient = GAUSSIAN_KERNEL[ky + R][kx + R];

                    // Pure vector processing chain with fixed, inherited constraints
                    vuint8m1_t pixels8 = __riscv_vle8_v_u8m1(row_ptr + kx, vl);
                    vuint16m2_t pixels16 = __riscv_vzext_vf2_u16m2(pixels8, vl);
                    
                    // Directly perform Widening-Multiply-Accumulate to eliminate the temporary product allocation track
                    accumulator = __riscv_vwmaccu_vx_u32m4(accumulator, coefficient, pixels16, vl);
                }
            }

            // Fixed-Point Normalization (acc = acc * 240)
            accumulator = __riscv_vmul_vx_u32m4(accumulator, 240u, vl);

            // Down-narrow via shift operations
            vuint16m2_t out16 = __riscv_vnsrl_wx_u16m2(accumulator, 16, vl);
            vuint8m1_t out8 = __riscv_vnsrl_wx_u8m1(out16, 0, vl);

            // Store back to output image segment
            __riscv_vse8_v_u8m1(&output.data[y * width + x], out8, vl);

            x += static_cast<int>(vl);
        }
    }
return output;
}
// ============================================================
// gaussianBlurRVV_LMUL2
// ============================================================
// Identical algorithm to gaussianBlurRVV but uses LMUL=2 instead of
// LMUL=4 for the accumulator (spec section 6.2 LMUL experiment).
//
// With LMUL=2: vuint32m2_t accumulator uses 2 physical registers
// instead of 4, leaving more logical registers available (16 vs 8).
// Tradeoff: fewer elements processed per vsetvl call (since pixels
// load at LMUL=1 fixed by the 8-bit width, but the widened types
// use m1 and m2 instead of m2 and m4).
Image gaussianBlurRVV_LMUL2(const Image& input) {
    int width  = input.width;
    int height = input.height;

    Image output;
    output.width  = width;
    output.height = height;
    output.data.resize(width * height, 0);

    // Border handling identical to LMUL=4 version
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if ((x < R) || (x >= width - R) || (y < R) || (y >= height - R)) {
                output.data[y * width + x] = computeGaussianPixelScalarBorder(input, x, y);
            }
        }
    }

    // Interior processing with LMUL=2
    for (int y = R; y < height - R; y++) {
        int x = R;
        while (x < width - R) {
            size_t remainingPixels = static_cast<size_t>((width - R) - x);

            // vsetvl for LMUL=1 pixel loads (8-bit pixels stay m1 regardless)
            // but we constrain vl based on what LMUL=2 accumulator can hold
            size_t vl = __riscv_vsetvl_e8mf2(remainingPixels);

            // Accumulator at LMUL=2 instead of LMUL=4
            vuint32m2_t accumulator = __riscv_vmv_v_x_u32m2(0, vl);

            for (int ky = -R; ky <= R; ky++) {
                const uint8_t* row_ptr = &input.data[(y + ky) * width + x];
                for (int kx = -R; kx <= R; kx++) {
                    uint16_t coefficient = GAUSSIAN_KERNEL[ky + R][kx + R];

                    // Load pixels at LMUL=mf2 (half of m1) to match the
                    // narrower accumulator chain at LMUL=2
                    vuint8mf2_t pixels8 = __riscv_vle8_v_u8mf2(row_ptr + kx, vl);
                    vuint16m1_t pixels16 = __riscv_vzext_vf2_u16m1(pixels8, vl);

                    accumulator = __riscv_vwmaccu_vx_u32m2(accumulator, coefficient, pixels16, vl);
                }
            }

            accumulator = __riscv_vmul_vx_u32m2(accumulator, 240u, vl);
            vuint16m1_t out16 = __riscv_vnsrl_wx_u16m1(accumulator, 16, vl);
            vuint8mf2_t out8  = __riscv_vnsrl_wx_u8mf2(out16, 0, vl);

            __riscv_vse8_v_u8mf2(&output.data[y * width + x], out8, vl);

            x += static_cast<int>(vl);
        }
    }

    return output;
}
// ============================================================
// gaussianBlurRVV_LMUL1
// ============================================================
// Same algorithm as gaussianBlurRVV but accumulator uses LMUL=1
// instead of LMUL=4 (spec section 6.2 LMUL experiment).
//
// With LMUL=1: vuint32m1_t accumulator uses only 1 physical register,
// leaving the most logical registers available (32) but processing
// the fewest elements per vsetvl call. To keep the widening chain
// consistent (8-bit -> 16-bit -> 32-bit, each step doubling LMUL),
// pixels must load at LMUL=1/4 (mf4) so after two widens we land at m1.
Image gaussianBlurRVV_LMUL1(const Image& input) {
    int width  = input.width;
    int height = input.height;

    Image output;
    output.width  = width;
    output.height = height;
    output.data.resize(width * height, 0);

    // Border handling identical to other LMUL versions
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if ((x < R) || (x >= width - R) || (y < R) || (y >= height - R)) {
                output.data[y * width + x] = computeGaussianPixelScalarBorder(input, x, y);
            }
        }
    }

    // Interior processing with LMUL=1
    for (int y = R; y < height - R; y++) {
        int x = R;
        while (x < width - R) {
            size_t remainingPixels = static_cast<size_t>((width - R) - x);

            // vsetvl at mf4 (quarter LMUL) for 8-bit pixels, so that after
            // two widening steps (8->16->32) the accumulator lands at LMUL=1
            size_t vl = __riscv_vsetvl_e8mf4(remainingPixels);

            // Accumulator at LMUL=1
            vuint32m1_t accumulator = __riscv_vmv_v_x_u32m1(0, vl);

            for (int ky = -R; ky <= R; ky++) {
                const uint8_t* row_ptr = &input.data[(y + ky) * width + x];
                for (int kx = -R; kx <= R; kx++) {
                    uint16_t coefficient = GAUSSIAN_KERNEL[ky + R][kx + R];

                    // Load pixels at mf4, widen to mf2, accumulate at m1
                    vuint8mf4_t pixels8  = __riscv_vle8_v_u8mf4(row_ptr + kx, vl);
                    vuint16mf2_t pixels16 = __riscv_vzext_vf2_u16mf2(pixels8, vl);

                    accumulator = __riscv_vwmaccu_vx_u32m1(accumulator, coefficient, pixels16, vl);
                }
            }

            accumulator = __riscv_vmul_vx_u32m1(accumulator, 240u, vl);
            vuint16mf2_t out16 = __riscv_vnsrl_wx_u16mf2(accumulator, 16, vl);
            vuint8mf4_t out8   = __riscv_vnsrl_wx_u8mf4(out16, 0, vl);

            __riscv_vse8_v_u8mf4(&output.data[y * width + x], out8, vl);

            x += static_cast<int>(vl);
        }
    }

    return output;
}
