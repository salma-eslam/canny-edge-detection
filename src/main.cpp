#include <iostream>
#include "types.h"
#include "direction.h"

int main() {
    std::cout << "=== Canny Edge Detection Pipeline  ===" << std::endl;

    //  a fake 2x2 gradient input image
    Image16 gx;
    gx.width = 2;
    gx.height = 2;
    gx.data = {100, 0,  5,  -50}; // Pure horizontal, Flat, Pure vertical, Diagonal 135°

    Image16 gy;
    gy.width = 2;
    gy.height = 2;
    gy.data = {0,   0, 200,  50}; // Perpendicular matching vectors

    std::cout << "Running gradient quantization engine..." << std::endl;
    Image result = gradientDirection(gx, gy);

    // Printing the results to verify the tags
    std::cout << "Pixel [0] (Expected 0): " << (int)result.data[0] << std::endl;
    std::cout << "Pixel [1] (Expected 0): " << (int)result.data[1] << std::endl;
    std::cout << "Pixel [2] (Expected 2): " << (int)result.data[2] << std::endl;
    std::cout << "Pixel [3] (Expected 3): " << (int)result.data[3] << std::endl;

	    return 0;
}
