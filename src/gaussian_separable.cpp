#include "gaussian_separable.h"
#include <algorithm>
#include <cstdint>

// 1D kernel used for both horizontal and vertical passes.
// This is the square root decomposition of a separable Gaussian:
// outer product of this kernel with itself approximates a 2D Gaussian.
// Sum = 17, so combined normalization after both passes is 17*17=289.
static const int16_t KERNEL_1D[5] = {1, 4, 7, 4, 1};
static const int32_t KERNEL_1D_SUM = 17;
static const int R = 2; // kernel radius

// gaussianBlurSeparable
// Pass 1 (horizontal): convolve each row with the 1D kernel along x.
// Pass 2 (vertical): convolve the intermediate result with the 1D
// kernel along y. Each pass does 5 multiply-accumulates per pixel
// instead of 25 for the full 2D version — total 10 instead of 25.
//
// Boundary handling: zero-padding in both passes, same approach as
// the 2D version, for consistency and to keep vectorization simple
// if this were ported to RVV later.
Image gaussianBlurSeparable(const Image& input) {
    int W = input.width;
    int H = input.height;

    // --------------------------------------------------------
    // Pass 1: Horizontal blur
    // For each pixel, sum 5 neighbors along the x-axis weighted
    // by KERNEL_1D, then divide by KERNEL_1D_SUM (17) and clamp.
    // Store as an intermediate Image (not yet final output).
    // --------------------------------------------------------
    Image horizontal;
    horizontal.width  = W;
    horizontal.height = H;
    horizontal.data.resize(W * H, 0);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int32_t acc = 0;
            for (int k = -R; k <= R; k++) {
                int srcX = x + k;
                if (srcX < 0 || srcX >= W) continue; // zero-padding
                acc += (int32_t)input.data[y * W + srcX]
                     * (int32_t)KERNEL_1D[k + R];
            }
            int32_t result = acc / KERNEL_1D_SUM;
            horizontal.data[y * W + x] =
                (uint8_t)std::clamp(result, 0, 255);
        }
    }

    // --------------------------------------------------------
    // Pass 2: Vertical blur
    // Take the horizontally-blurred intermediate image and convolve
    // along the y-axis with the same 1D kernel.
    // --------------------------------------------------------
    Image output;
    output.width  = W;
    output.height = H;
    output.data.resize(W * H, 0);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int32_t acc = 0;
            for (int k = -R; k <= R; k++) {
                int srcY = y + k;
                if (srcY < 0 || srcY >= H) continue; // zero-padding
                acc += (int32_t)horizontal.data[srcY * W + x]
                     * (int32_t)KERNEL_1D[k + R];
            }
            int32_t result = acc / KERNEL_1D_SUM;
            output.data[y * W + x] =
                (uint8_t)std::clamp(result, 0, 255);
        }
    }

    return output;
}
