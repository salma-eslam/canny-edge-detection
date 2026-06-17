#pragma once
#include "types.h"

// Scalar Baselines
Image magnitudeL1(const Image16& gx, const Image16& gy);
Image magnitudeL2(const Image16& gx, const Image16& gy);

// RVV Vector Optimizations
Image magnitudeL1Rvv(const Image16& gx, const Image16& gy);
Image magnitudeL2Rvv(const Image16& gx, const Image16& gy);
