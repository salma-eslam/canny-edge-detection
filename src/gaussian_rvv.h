#ifndef GAUSSIAN_RVV_H
#define GAUSSIAN_RVV_H

#include "types.h"

Image gaussianBlurRVV(const Image& input);
void gaussianBlurRVV(const Image& input, Image& output);

Image gaussianBlurRVV_LMUL2(const Image& input);
void gaussianBlurRVV_LMUL2(const Image& input, Image& output);

Image gaussianBlurRVV_LMUL1(const Image& input);
void gaussianBlurRVV_LMUL1(const Image& input, Image& output);

#endif

