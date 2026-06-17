#pragma once
#include "types.h"

Image16 sobelX(const Image& input);
Image16 sobelY(const Image& input);

// This version calculates both Gx and Gy simultaneously in a single pass to maximize cache efficiency.
void sobel_fused_rvv(const Image& input, Image16& out_gx, Image16& out_gy);
