#pragma once
#include "types.h"

// gaussianBlurSeparableRVV
// RVV-vectorized version of the separable Gaussian filter.
// Each pass (horizontal, vertical) is vectorized across the x-axis
// using strip-mining, same technique as gaussianBlurRVV but with
// only 5 kernel taps per pass instead of 25 (since separable).
Image gaussianBlurSeparableRVV(const Image& input);

