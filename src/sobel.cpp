#include "sobel.h"
#include <cstdlib>  // for abs()

// Sobel kernel used to compute the gradient in the x-direction (Gx).
// The values on the left side are negative and the values on the
// right side are positive. During convolution, this kernel measures
// how much the pixel intensity changes from left to right.
// A large absolute value of Gx indicates a strong vertical edge.
static const int KX[3][3] = {
    {-1,  0, +1},
    {-2,  0, +2},
    {-1,  0, +1}
};

// Sobel kernel used to compute the gradient in the y-direction (Gy).
// The values in the top row are negative and the values in the
// bottom row are positive. During convolution, this kernel measures
// how much the pixel intensity changes from top to bottom.
// A large absolute value of Gy indicates a strong horizontal edge.
static const int KY[3][3] = {
    {-1, -2, -1},
    { 0,  0,  0},
    {+1, +2, +1}
};





// Apply the Sobel kernel in the x-direction (Gx).
// This function calculates how much the image intensity changes
// from left to right. Large values indicate a strong vertical edge.
// The result is stored in Image16 because Sobel values can become
// negative and may exceed the range of an 8-bit image.
Image16 sobelX(const Image& input) {

    // Create an output image with the same width and height
    // as the original image.
    Image16 output;
    output.width  = input.width;
    output.height = input.height;

    // Allocate memory for all pixels and initialize them to zero.
    output.data.resize(input.width * input.height, 0);

    // Traverse all pixels except the outer border.
    // Border pixels are skipped because a 3x3 Sobel kernel
    // needs one neighboring pixel on every side, which does
    // not exist at the image boundaries.
    for (int y = 1; y < input.height - 1; y++) {
        for (int x = 1; x < input.width - 1; x++) {

            // This variable stores the convolution result
            // for the current pixel.
            int sum = 0;

            // Iterate through the 3x3 neighborhood centered
            // around the current pixel.
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {

                    // Access a neighboring pixel.
                    // (x + kx, y + ky) moves around the current pixel
                    // to cover all locations inside the 3x3 window.
                    int pixel = input.data[
                        (y + ky) * input.width + (x + kx)
                    ];

                    // Multiply the neighboring pixel by the
                    // corresponding Sobel kernel coefficient.
                    // Each coefficient contributes differently
                    // to the gradient calculation depending on
                    // its position in the kernel.
                    sum += pixel * KX[ky + 1][kx + 1];
                }
            }

            // After processing all 9 neighboring pixels,
            // 'sum' contains the horizontal gradient (Gx)
            // at the current location.
            //
            // Positive values mean intensity generally increases
            // from left to right, while negative values mean it
            // decreases from left to right.
            //
            // The magnitude of the value indicates how strong
            // the edge is at this pixel.
            output.data[y * output.width + x] =
                static_cast<int16_t>(sum);
        }
    }

    // Return the image containing all Gx gradient values.
    // This image can later be combined with Gy to compute
    // the final Sobel edge magnitude.
    return output;
}

























// Apply the Sobel kernel in the y-direction (Gy).
// This function calculates how much the image intensity changes
// from top to bottom. Large values indicate a strong horizontal edge.
// The result is stored in Image16 because Sobel values can become
// negative and may exceed the range of an 8-bit image.
Image16 sobelY(const Image& input) {

    // Create an output image with the same width and height
    // as the original image.
    Image16 output;
    output.width  = input.width;
    output.height = input.height;

    // Allocate memory for all pixels and initialize them to zero.
    output.data.resize(input.width * input.height, 0);

    // Traverse all pixels except the outer border.
    // Border pixels are skipped because a 3x3 Sobel kernel
    // needs one neighboring pixel on every side, which does
    // not exist at the image boundaries.
    for (int y = 1; y < input.height - 1; y++) {
        for (int x = 1; x < input.width - 1; x++) {

            // This variable stores the convolution result
            // for the current pixel.
            int sum = 0;

            // Iterate through the 3x3 neighborhood centered
            // around the current pixel.
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {

                    // Access a neighboring pixel.
                    int pixel = input.data[
                        (y + ky) * input.width + (x + kx)
                    ];

                    // Multiply the neighboring pixel by the
                    // corresponding Sobel kernel coefficient.
                    // KY detects vertical changes instead of horizontal.
                    sum += pixel * KY[ky + 1][kx + 1];
                }
            }

            // After processing all 9 neighboring pixels,
            // 'sum' contains the vertical gradient (Gy)
            // at the current location.
            //
            // Positive values mean intensity generally increases
            // from top to bottom, while negative values mean it
            // decreases from top to bottom.
            output.data[y * output.width + x] =
                static_cast<int16_t>(sum);
        }
    }

    // Return the image containing all Gy gradient values.
    // This image can later be combined with Gx to compute
    // the final Sobel edge magnitude.
    return output;
}
































