#include "types.h"
#include "gaussian.h"
#include "gaussian_separable.h"
#include <iostream>
#include <chrono>
#include <cstdint>

// createTestImage
// Deterministic synthetic test image, same pattern used in other benchmarks
static Image createTestImage(int width, int height) {
    Image input;
    input.width = width;
    input.height = height;
    input.data.resize(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            input.data[y * width + x] =
                static_cast<uint8_t>((x * 3 + y * 5) % 256);
        }
    }
    return input;
}

int main() {
    const int width = 512;
    const int height = 512;
    const int iterations = 100;

    std::cout << "=== Standard 2D vs Separable Gaussian Benchmark ===\n";
    std::cout << "Image size: " << width << "x" << height << "\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    Image input = createTestImage(width, height);

    // Warm-up
    volatile Image warm2D  = gaussianBlur(input);
    volatile Image warmSep = gaussianBlurSeparable(input);

    // Timed run: standard 2D convolution (25 multiply-accumulates/pixel)
    auto start2D = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        volatile Image temp = gaussianBlur(input);
    }
    auto end2D = std::chrono::high_resolution_clock::now();

    // Timed run: separable convolution (10 multiply-accumulates/pixel)
    auto startSep = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        volatile Image temp = gaussianBlurSeparable(input);
    }
    auto endSep = std::chrono::high_resolution_clock::now();

    double ms2D = std::chrono::duration<double, std::milli>(end2D - start2D).count() / iterations;
    double msSep = std::chrono::duration<double, std::milli>(endSep - startSep).count() / iterations;

    std::cout << "Standard_2D_ms:   " << ms2D  << "\n";
    std::cout << "Separable_ms:     " << msSep << "\n";
    std::cout << "Speedup_Separable_vs_2D: " << (ms2D / msSep) << "x\n";

    return 0;
}
