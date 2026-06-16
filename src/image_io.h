#pragma once 
// Prevents this header from being included more than once
// even if multiple .cpp files include it
#include "types.h"
#include <string>
// Loads a raw grayscale image from disk
// The file must be exactly width*height bytes (no headers, no compression)
Image loadRawImage(const std::string& filename, int width, int height);
// Saves a raw grayscale image to disk
// Writes exactly width*height bytes with no headers
void saveRawImage(const std::string& filename, const Image& image);
