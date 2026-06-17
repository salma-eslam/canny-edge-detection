#pragma once
#include "types.h"

// gaussianBlurSeparable
// Separable 5x5 Gaussian blur: decomposes the 2D convolution into
// a 1x5 horizontal pass followed by a 5x1 vertical pass.
//
// Uses 1D kernel [1, 4, 7, 4, 1] (sum=17) applied twice via outer
// product, giving total normalization factor 17*17=289. This is a
// different (but valid) Gaussian approximation than our hand-tuned
// 2D kernel (sum=273) — true separable kernels are outer products
// by construction, while our 2D kernel was tuned independently and
// is not an exact outer product (verified via SVD decomposition).
//
// Reduces multiply-accumulate operations from 25 per pixel (2D) to
// 10 per pixel (5 horizontal + 5 vertical), per spec "Deeper Idea:
// Separable Filters" section 2.2.
Image gaussianBlurSeparable(const Image& input);
