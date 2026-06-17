#include "gaussian_separable_rvv.h"
#include <riscv_vector.h>
#include <cstdint>
#include <algorithm>

// Same 1D kernel as the scalar separable version: [1,4,7,4,1], sum=17.
// Kept as a local static here (duplicated from gaussian_separable.cpp)
// so this file is self-contained and independently compilable for
// the RISC-V cross-compiler without depending on internal statics
// from another translation unit.
static const int16_t KERNEL_1D[5] = {1, 4, 7, 4, 1};
static const uint32_t KERNEL_1D_SUM = 17;
static const int R = 2;

// ============================================================
// gaussianBlurSeparableRVV
// ============================================================
// RVV-vectorized separable Gaussian blur, combining two optimization
// techniques: (1) separable decomposition reduces multiply-accumulates
// from 25 to 10 per pixel, (2) RVV vectorizes each pass across the
// x-axis using strip-mining. This is an "above and beyond" experiment
// beyond the spec's required RVV work (Gaussian 2D), exploring whether
// these two optimizations compose well together.
//
// Pass 1 (horizontal): vectorize across x within each row, 5 taps.
// Pass 2 (vertical): vectorize across x again, but each kernel tap
// reads from a different row (y+k) at the same x position.
Image gaussianBlurSeparableRVV(const Image& input) {
    int W = input.width;
    int H = input.height;

    // --------------------------------------------------------
    // Pass 1: Horizontal blur (vectorized across x)
    // --------------------------------------------------------
    Image horizontal;
    horizontal.width  = W;
    horizontal.height = H;
    horizontal.data.resize(W * H, 0);

    for (int y = 0; y < H; y++) {

        // Scalar border handling for left/right 2 columns
        for (int x = 0; x < W; x++) {
            if (x >= R && x < W - R) continue;
            int32_t acc = 0;
            for (int k = -R; k <= R; k++) {
                int srcX = x + k;
                if (srcX < 0 || srcX >= W) continue;
                acc += (int32_t)input.data[y * W + srcX] * (int32_t)KERNEL_1D[k + R];
            }
            int32_t result = acc / (int32_t)KERNEL_1D_SUM;
            horizontal.data[y * W + x] = (uint8_t)std::clamp(result, 0, 255);
        }

        // Vectorized interior columns
        int x = R;
        while (x < W - R) {
            size_t vl = __riscv_vsetvl_e8m1(W - R - x);

            vuint32m4_t acc = __riscv_vmv_v_x_u32m4(0, vl);

            for (int k = -R; k <= R; k++) {
                uint16_t coeff = (uint16_t)KERNEL_1D[k + R];
                const uint8_t* srcPtr = input.data.data() + y * W + (x + k);

                vuint8m1_t pixels8   = __riscv_vle8_v_u8m1(srcPtr, vl);
                vuint16m2_t pixels16 = __riscv_vzext_vf2_u16m2(pixels8, vl);

                acc = __riscv_vwmaccu_vx_u32m4(acc, coeff, pixels16, vl);
            }

            // Divide by 17 directly (vector-vector division by a
            // broadcast scalar vector), since 17 has no clean
            // power-of-2 fixed-point shortcut like 273 does.
            vuint32m4_t divisor = __riscv_vmv_v_x_u32m4(KERNEL_1D_SUM, vl);
            acc = __riscv_vdivu_vv_u32m4(acc, divisor, vl);

            vuint16m2_t out16 = __riscv_vnsrl_wx_u16m2(acc, 0, vl);
            vuint8m1_t out8   = __riscv_vnsrl_wx_u8m1(out16, 0, vl);

            __riscv_vse8_v_u8m1(horizontal.data.data() + y * W + x, out8, vl);

            x += static_cast<int>(vl);
        }
    }

    // --------------------------------------------------------
    // Pass 2: Vertical blur (vectorized across x, reading 5 rows)
    // --------------------------------------------------------
    Image output;
    output.width  = W;
    output.height = H;
    output.data.resize(W * H, 0);

    for (int y = 0; y < H; y++) {

        // Scalar border handling for top/bottom 2 rows
        if (y < R || y >= H - R) {
            for (int x = 0; x < W; x++) {
                int32_t acc = 0;
                for (int k = -R; k <= R; k++) {
                    int srcY = y + k;
                    if (srcY < 0 || srcY >= H) continue;
                    acc += (int32_t)horizontal.data[srcY * W + x] * (int32_t)KERNEL_1D[k + R];
                }
                int32_t result = acc / (int32_t)KERNEL_1D_SUM;
                output.data[y * W + x] = (uint8_t)std::clamp(result, 0, 255);
            }
            continue;
        }

        // Vectorized interior rows: process all x positions in strips
        int x = 0;
        while (x < W) {
            size_t vl = __riscv_vsetvl_e8m1(W - x);

            vuint32m4_t acc = __riscv_vmv_v_x_u32m4(0, vl);

            for (int k = -R; k <= R; k++) {
                uint16_t coeff = (uint16_t)KERNEL_1D[k + R];
                const uint8_t* srcPtr = horizontal.data.data() + (y + k) * W + x;

                vuint8m1_t pixels8   = __riscv_vle8_v_u8m1(srcPtr, vl);
                vuint16m2_t pixels16 = __riscv_vzext_vf2_u16m2(pixels8, vl);

                acc = __riscv_vwmaccu_vx_u32m4(acc, coeff, pixels16, vl);
            }

            vuint32m4_t divisor = __riscv_vmv_v_x_u32m4(KERNEL_1D_SUM, vl);
            acc = __riscv_vdivu_vv_u32m4(acc, divisor, vl);

            vuint16m2_t out16 = __riscv_vnsrl_wx_u16m2(acc, 0, vl);
            vuint8m1_t out8   = __riscv_vnsrl_wx_u8m1(out16, 0, vl);

            __riscv_vse8_v_u8m1(output.data.data() + y * W + x, out8, vl);

            x += static_cast<int>(vl);
        }
    }

    return output;
}
