#pragma once
#include "types.h"  
#include <vector>    
#include <algorithm> 

// Generic 2D convolution.
// For every output pixel:
//   - Place the kernel over the corresponding input location.
//   - Multiply each overlapping pixel by its kernel coefficient.
//   - Add all products together.
//   - Normalize by dividing by kernelSum.
//   - Clamp the final value to the valid pixel range [0, 255].
//
// Template parameters:
//   PixelType  : Image pixel type 
//   AccType    : Accumulator type used during convolution
//                int32_t is large enough to safely hold intermediate sums.
//   KernelType : Kernel coefficient type (int16_t is sufficient for
//                small integer kernels such as Gaussian filters)
template<typename PixelType, typename AccType, typename KernelType>
Image convolve2D(
    const Image& input,
    const std::vector<std::vector<KernelType>>& kernel,
    AccType kernelSum
) {
    // Image dimensions
    int width  = input.width;
    int height = input.height;

    // Kernel dimensions
    int kernelHeight = kernel.size();
    int kernelWidth  = kernel[0].size();

    // Location of the kernel center.
    int kernelCenterY = kernelHeight / 2;
    int kernelCenterX = kernelWidth / 2;

    // Output image has the same size as the input image.
    Image output;
    output.width  = width;
    output.height = height;
    output.data.resize(width * height);

    // Process every output pixel.
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // Accumulates the weighted sum for the current output pixel.
            AccType accumulator = 0;

            // Iterate through every kernel coefficient.
            for (int ky = 0; ky < kernelHeight; ky++) {
                for (int kx = 0; kx < kernelWidth; kx++) {

                
                    int srcY = y + ky - kernelCenterY;
                    int srcX = x + kx - kernelCenterX;

                    // Zero-padding boundary handling:
                    // Pixels outside the image are treated as zero.
                    if (srcY < 0 || srcY >= height ||
                        srcX < 0 || srcX >= width) {
                        continue;
                    }
                    // Cast to AccType before multiplication to avoid overflow 
                    accumulator +=
                        (AccType)input.data[srcY * width + srcX] *
                        (AccType)kernel[ky][kx];
                }
            }

            // Normalize the accumulated sum.
            AccType normalizedValue = accumulator / kernelSum;

            // Clamp to the valid grayscale range and store the result.
            output.data[y * width + x] =
                (PixelType)std::clamp(
                    normalizedValue,
                    (AccType)0,
                    (AccType)255
                );
        }
    }

    return output;
}
