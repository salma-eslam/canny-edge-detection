#include "gaussian.h"
#include "convolution.h" // for convolve2D template

// gaussianBlur
// Applies a 5x5 Gaussian blur to the input image.
// Define the 5x5 integer kernel (coefficients sum to 273)
// Call convolve2D with the kernel and kernelSum=273
// convolve2D handles zero-padding, accumulation, normalization and clamping
Image gaussianBlur(const Image& input) {

    std::vector<std::vector<int16_t>> kernel = {
    { 1,  4,  7,  4,  1},
    { 4, 16, 26, 16,  4},
    { 7, 26, 41, 26,  7},
    { 4, 16, 26, 16,  4},
    { 1,  4,  7,  4,  1}
};

    // kernelSum = 273 = sum of all 25 coefficients
    // We divide by this after accumulation to normalize the result
    // back to [0, 255] range
    int32_t kernelSum = 273;

    // Call the generic convolution template with:
    //   PixelType  = uint8_t as output pixels are grayscale (0 to 255)
    //   AccType    = int32_t because accumulator holds up to 95,625 safely
    //   KernelType = int16_t
    return convolve2D<uint8_t, int32_t, int16_t>(input, kernel, kernelSum);
}
