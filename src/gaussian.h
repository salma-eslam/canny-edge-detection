#pragma once
// gaussian.h — declaration of the Gaussian blur function
// the implementation is in gaussian.cpp

#include "types.h" // for the Image struct

// gaussianBlur
// Applies a 5x5 Gaussian blur to the input image to reduce noise.
// Blurring is done before edge detection so that small noise pixels
// don't get mistakenly detected as edges by the Sobel operator
Image gaussianBlur(const Image& input);

