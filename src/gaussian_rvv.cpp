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
    // Fixed-point approximation of division by 273:
    // output ≈ sum * 240 / 65536
    //
    // Maximum possible sum is approximately:
    // 255 * 273 = 69615
    // 69615 * 240 = 16,707,600
    //
    // This safely fits in uint32_t.
    uint32_t value = (sum * 240u) >> 16;
    return static_cast<uint8_t>(std::clamp<uint32_t>(value, 0, 255));
}

static uint8_t computeGaussianPixelScalarBorder(const Image& input, int x, int y) {
    const int W = input.width;
    const int H = input.height;
    uint32_t accumulator = 0;

    for (int ky = -R; ky <= R; ky++) {
        for (int kx = -R; kx <= R; kx++) {
            int sourceX = x + kx;
            int sourceY = y + ky;

            if (sourceX < 0 || sourceX >= W || sourceY < 0 || sourceY >= H) {
                continue;
            }

            uint8_t pixel = input.data[sourceY * W + sourceX];
            uint16_t coefficient = GAUSSIAN_KERNEL[ky + R][kx + R];

            accumulator += static_cast<uint32_t>(pixel) *
                           static_cast<uint32_t>(coefficient);
        }
    }

    return normalizeApproxScalar(accumulator);
}

static void prepareOutput(const Image& input, Image& output) {
    output.width = input.width;
    output.height = input.height;

    const size_t requiredSize = static_cast<size_t>(input.width) *
                                static_cast<size_t>(input.height);

    if (output.data.size() != requiredSize) {
        output.data.resize(requiredSize);
    }
}

static void handleBordersScalar(const Image& input, Image& output) {
    const int width = input.width;
    const int height = input.height;

    // If the image is too small for a 5x5 interior region,
    // process everything with the scalar border-safe function.
    if (width <= 2 * R || height <= 2 * R) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                output.data[y * width + x] =
                    computeGaussianPixelScalarBorder(input, x, y);
            }
        }
        return;
    }

    // Top border
    for (int y = 0; y < R; y++) {
        for (int x = 0; x < width; x++) {
            output.data[y * width + x] =
                computeGaussianPixelScalarBorder(input, x, y);
        }
    }

    // Bottom border
    for (int y = height - R; y < height; y++) {
        for (int x = 0; x < width; x++) {
            output.data[y * width + x] =
                computeGaussianPixelScalarBorder(input, x, y);
        }
    }

    // Left and right borders, excluding rows already handled above
    for (int y = R; y < height - R; y++) {
        for (int x = 0; x < R; x++) {
            output.data[y * width + x] =
                computeGaussianPixelScalarBorder(input, x, y);
        }

        for (int x = width - R; x < width; x++) {
            output.data[y * width + x] =
                computeGaussianPixelScalarBorder(input, x, y);
        }
    }
}

// ============================================================
// gaussianBlurRVV - LMUL=4 version
// ============================================================

void gaussianBlurRVV(const Image& input, Image& output) {
    const int width = input.width;
    const int height = input.height;

    prepareOutput(input, output);
    handleBordersScalar(input, output);

    for (int y = R; y < height - R; y++) {
        int x = R;

        while (x < width - R) {
            size_t remainingPixels =
                static_cast<size_t>((width - R) - x);

            size_t vl = __riscv_vsetvl_e8m1(remainingPixels);

            vuint32m4_t accumulator =
                __riscv_vmv_v_x_u32m4(0, vl);

            #pragma GCC unroll 5
            for (int ky = -R; ky <= R; ky++) {
                const uint8_t* row_ptr =
                    &input.data[(y + ky) * width + x];

                #pragma GCC unroll 5
                for (int kx = -R; kx <= R; kx++) {
                    uint16_t coefficient =
                        GAUSSIAN_KERNEL[ky + R][kx + R];

                    vuint8m1_t pixels8 =
                        __riscv_vle8_v_u8m1(row_ptr + kx, vl);

                    vuint16m2_t pixels16 =
                        __riscv_vzext_vf2_u16m2(pixels8, vl);

                    accumulator =
                        __riscv_vwmaccu_vx_u32m4(
                            accumulator,
                            coefficient,
                            pixels16,
                            vl
                        );
                }
            }

            // Fixed-point approximation of division by 273:
            // output ≈ accumulator * 240 / 65536
            //
            // Max accumulator ≈ 255 * 273 = 69615.
            // 69615 * 240 = 16,707,600, which safely fits in uint32_t.
            accumulator =
                __riscv_vmul_vx_u32m4(accumulator, 240u, vl);

            vuint16m2_t out16 =
                __riscv_vnsrl_wx_u16m2(accumulator, 16, vl);

            vuint8m1_t out8 =
                __riscv_vnsrl_wx_u8m1(out16, 0, vl);

            __riscv_vse8_v_u8m1(
                &output.data[y * width + x],
                out8,
                vl
            );

            x += static_cast<int>(vl);
        }
    }
}

Image gaussianBlurRVV(const Image& input) {
    Image output;
    gaussianBlurRVV(input, output);
    return output;
}

// ============================================================
// gaussianBlurRVV_LMUL2
// ============================================================

void gaussianBlurRVV_LMUL2(const Image& input, Image& output) {
    const int width = input.width;
    const int height = input.height;

    prepareOutput(input, output);
    handleBordersScalar(input, output);

    for (int y = R; y < height - R; y++) {
        int x = R;

        while (x < width - R) {
            size_t remainingPixels =
                static_cast<size_t>((width - R) - x);

            size_t vl = __riscv_vsetvl_e8mf2(remainingPixels);

            vuint32m2_t accumulator =
                __riscv_vmv_v_x_u32m2(0, vl);

            #pragma GCC unroll 5
            for (int ky = -R; ky <= R; ky++) {
                const uint8_t* row_ptr =
                    &input.data[(y + ky) * width + x];

                #pragma GCC unroll 5
                for (int kx = -R; kx <= R; kx++) {
                    uint16_t coefficient =
                        GAUSSIAN_KERNEL[ky + R][kx + R];

                    vuint8mf2_t pixels8 =
                        __riscv_vle8_v_u8mf2(row_ptr + kx, vl);

                    vuint16m1_t pixels16 =
                        __riscv_vzext_vf2_u16m1(pixels8, vl);

                    accumulator =
                        __riscv_vwmaccu_vx_u32m2(
                            accumulator,
                            coefficient,
                            pixels16,
                            vl
                        );
                }
            }

            accumulator =
                __riscv_vmul_vx_u32m2(accumulator, 240u, vl);

            vuint16m1_t out16 =
                __riscv_vnsrl_wx_u16m1(accumulator, 16, vl);

            vuint8mf2_t out8 =
                __riscv_vnsrl_wx_u8mf2(out16, 0, vl);

            __riscv_vse8_v_u8mf2(
                &output.data[y * width + x],
                out8,
                vl
            );

            x += static_cast<int>(vl);
        }
    }
}

Image gaussianBlurRVV_LMUL2(const Image& input) {
    Image output;
    gaussianBlurRVV_LMUL2(input, output);
    return output;
}

// ============================================================
// gaussianBlurRVV_LMUL1
// ============================================================

void gaussianBlurRVV_LMUL1(const Image& input, Image& output) {
    const int width = input.width;
    const int height = input.height;

    prepareOutput(input, output);
    handleBordersScalar(input, output);

    for (int y = R; y < height - R; y++) {
        int x = R;

        while (x < width - R) {
            size_t remainingPixels =
                static_cast<size_t>((width - R) - x);

            size_t vl = __riscv_vsetvl_e8mf4(remainingPixels);

            vuint32m1_t accumulator =
                __riscv_vmv_v_x_u32m1(0, vl);

            #pragma GCC unroll 5
            for (int ky = -R; ky <= R; ky++) {
                const uint8_t* row_ptr =
                    &input.data[(y + ky) * width + x];

                #pragma GCC unroll 5
                for (int kx = -R; kx <= R; kx++) {
                    uint16_t coefficient =
                        GAUSSIAN_KERNEL[ky + R][kx + R];

                    vuint8mf4_t pixels8 =
                        __riscv_vle8_v_u8mf4(row_ptr + kx, vl);

                    vuint16mf2_t pixels16 =
                        __riscv_vzext_vf2_u16mf2(pixels8, vl);

                    accumulator =
                        __riscv_vwmaccu_vx_u32m1(
                            accumulator,
                            coefficient,
                            pixels16,
                            vl
                        );
                }
            }

            accumulator =
                __riscv_vmul_vx_u32m1(accumulator, 240u, vl);

            vuint16mf2_t out16 =
                __riscv_vnsrl_wx_u16mf2(accumulator, 16, vl);

            vuint8mf4_t out8 =
                __riscv_vnsrl_wx_u8mf4(out16, 0, vl);

            __riscv_vse8_v_u8mf4(
                &output.data[y * width + x],
                out8,
                vl
            );

            x += static_cast<int>(vl);
        }
    }
}

Image gaussianBlurRVV_LMUL1(const Image& input) {
    Image output;
    gaussianBlurRVV_LMUL1(input, output);
    return output;
}
