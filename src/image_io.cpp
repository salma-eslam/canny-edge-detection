#include "image_io.h"
#include <cstdio>     
#include <cstdlib>    // for aligned_alloc, free
#include <cstring>    // for memcpy
#include <stdexcept>  

// loadRawImage
// This function opens a file on disk and reads it into memory
// as an Image struct. The file must be a "raw" grayscale image
// it conatins (ONLY pixel values) just width*height bytes, one byte per pixel,
// stored row by row from top-left to bottom-right.
// The caller must know the width and height in advance because
// the file itself contains no size information 
Image loadRawImage(const std::string& filename, int width, int height) {

    // fopen opens the file at the given path.
    // rb mode which means read binary
    // c_str() is needed because fopen expects an old-style C char* pointer,
    // but we have a modern C++ std::string, c_str() gives us that pointer.
    FILE* fileHandle = fopen(filename.c_str(), "rb");

    // If fopen returns NULL, it means the file doesn't exist,
    if (!fileHandle) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    // aligned_alloc(64, size) allocates a 64-byte aligned buffer
    uint8_t* alignedBuffer = (uint8_t*)aligned_alloc(64, width * height);
    if (!alignedBuffer) {
        fclose(fileHandle);
        throw std::runtime_error("Failed to allocate aligned memory");
    }

    // fread reads raw bytes from the file into our memory buffer.
    // Arguments:
    //   img.data.data(): a raw pointer to the first byte of our vector's internal array      
    //  one pixel is one byte
    //   width * height 
    // After this call, img.data contains all the pixel values from the file.
    fread(alignedBuffer, 1, width * height, fileHandle);

    //  close the file when done.
    fclose(fileHandle);

    // Create an empty Image struct object and fill in its dimensions.
    Image img;
    img.width  = width;
    img.height = height;

    // Allocates  width*height bytes of memory inside the vector and fill every byte with 0 initially using resize
    img.data.resize(width * height);

    // memcpy copies pixel data from aligned buffer into the vector
    memcpy(img.data.data(), alignedBuffer, width * height);

    // Free the aligned buffer — always free what you aligned_alloc
    free(alignedBuffer);

    // Return the fully populated Image struct.
    return img;
}


// saveRawImage
// This function takes an Image struct from memory and writes
// its pixel data to a file on disk in raw format.
// The output file will be width*height bytes 
void saveRawImage(const std::string& filename, const Image& image) {

    // Open the file in binary write mode wb.
    // If the file doesn't exist yet it will be created automatically.
    // If the file already exists it will be completely overwritten.
    FILE* fileHandle = fopen(filename.c_str(), "wb");

    // If fopen returns NULL can't write to this location.
    if (!fileHandle) {
        throw std::runtime_error("Cannot write file: " + filename);
    }

    // aligned_alloc(64, size) allocates a 64-byte aligned buffer
    uint8_t* alignedBuffer = (uint8_t*)aligned_alloc(64, image.width * image.height);
    if (!alignedBuffer) {
        fclose(fileHandle);
        throw std::runtime_error("Failed to allocate aligned memory");
    }

    // memcpy copies pixel data from vector into the aligned buffer
    memcpy(alignedBuffer, image.data.data(), image.width * image.height);

    // fwrite writes raw bytes from our memory buffer to the file.
    // Arguments:
    //   image.data.data() : pointer to the first pixel in our vector
    //   each element is 1 byte
    //   image.width * image.height: total number of pixels to write
    fwrite(alignedBuffer, 1, image.width * image.height, fileHandle);

    // Free the aligned buffer
    free(alignedBuffer);

    // Close the file.
    fclose(fileHandle);
}
