#include "gradient.h"
#include <cmath>   // for sqrt()
#include <cstdlib> // for abs()

// Computes the gradient magnitude using the L1 norm.
// L1 norm = |Gx| + |Gy|
// This is faster than L2 because it avoids the square root.
// The result is normalized to fit in the 0-255 range.
Image magnitudeL1(const Image16& gx, const Image16& gy) {

    Image output;
    output.width  = gx.width;
    output.height = gx.height;
    output.data.resize(gx.width * gx.height, 0);

    // Step 1: compute raw L1 magnitude for every pixel
    // and find the maximum value at the same time
    int maxVal = 0;
    std::vector<int> raw(gx.width * gx.height, 0);

    for (int i = 0; i < gx.width * gx.height; i++) {
        // abs() gives us the absolute value (removes negative sign)
        raw[i] = abs(gx.data[i]) + abs(gy.data[i]);

        // Track the maximum value so we can normalize later
        if (raw[i] > maxVal) {
            maxVal = raw[i];
        }
    }

    // Step 2: normalize all values to 0-255 range
    // We divide each value by the maximum and multiply by 255
    // This keeps the relative differences but fits everything in 0-255
    if (maxVal > 0) {
        for (int i = 0; i < gx.width * gx.height; i++) {
            output.data[i] = static_cast<uint8_t>(
                (raw[i] * 255) / maxVal
            );
        }
    }

    return output;
}











// Computes the gradient magnitude using the L2 norm.
// L2 norm = sqrt(Gx² + Gy²)
// This is more accurate than L1 because it computes the true
// geometric length of the gradient vector.
// The result is normalized to fit in the 0-255 range.
Image magnitudeL2(const Image16& gx, const Image16& gy) {

    Image output;
    output.width  = gx.width;
    output.height = gx.height;
    output.data.resize(gx.width * gx.height, 0);

    // Step 1: compute raw L2 magnitude for every pixel
    // and find the maximum value at the same time.
    // We use double instead of int because sqrt() returns
    // a decimal number that cannot be stored in an integer.
    double maxVal = 0.0;
    std::vector<double> raw(gx.width * gx.height, 0.0);

    for (int i = 0; i < gx.width * gx.height; i++) {

        // We cast to double before squaring for two reasons:
        // 1. sqrt() returns a decimal number so we need double to store it
        // 2. int16_t * int16_t can overflow — for example if Gx = 1000,
        //    then Gx² = 1,000,000 which is too large for a 16-bit integer.
        //    double can safely hold numbers up to 1.7 × 10^308
        double gxVal = static_cast<double>(gx.data[i]);
        double gyVal = static_cast<double>(gy.data[i]);

        raw[i] = sqrt(gxVal * gxVal + gyVal * gyVal);

        // Track the maximum value so we can normalize later
        if (raw[i] > maxVal) {
            maxVal = raw[i];
        }
    }

    // Step 2: normalize all values to 0-255 range.
    // We divide each value by the maximum and multiply by 255.
    // This keeps the relative differences but fits everything in 0-255.
    if (maxVal > 0.0) {
        for (int i = 0; i < gx.width * gx.height; i++) {
            output.data[i] = static_cast<uint8_t>(
                (raw[i] / maxVal) * 255.0
            );
        }
    }

    return output;
}
