#include "types.h"
#include "gaussian.h"
#include "gaussian_separable.h"
#include "gaussian_separable_rvv.h"

#include <iostream>
#include <cstdint>
#include <cstdlib>

static inline uint64_t readCycle() {
    uint64_t cycles;
    asm volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

static Image createTestImage(int width, int height) {
    Image input;
    input.width = width;
    input.height = height;
    input.data.resize(width * height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            input.data[y * width + x] =
                static_cast<uint8_t>((x * 3 + y * 5 + (x * y) % 17) % 256);
        }
    }

    return input;
}

static uint64_t checksumImage(const Image& image) {
    uint64_t sum = 0;

    for (uint8_t pixel : image.data) {
        sum += pixel;
    }

    return sum;
}

static void compareImages(const Image& scalar, const Image& rvv) {
    int mismatches = 0;
    int maxDiff = 0;
    uint64_t totalDiff = 0;

    for (size_t i = 0; i < scalar.data.size(); i++) {
        int diff = std::abs(
            static_cast<int>(scalar.data[i]) -
            static_cast<int>(rvv.data[i])
        );

        if (diff != 0) {
            mismatches++;
        }

        if (diff > maxDiff) {
            maxDiff = diff;
        }

        totalDiff += static_cast<uint64_t>(diff);
    }

    double avgDiff =
        static_cast<double>(totalDiff) /
        static_cast<double>(scalar.data.size());

    std::cout << "Separable_RVV_vs_scalar_mismatches: " << mismatches << "\n";
    std::cout << "Separable_RVV_vs_scalar_maxDiff: " << maxDiff << "\n";
    std::cout << "Separable_RVV_vs_scalar_avgDiff: " << avgDiff << "\n\n";

    if (mismatches != 0) {
        std::cerr << "WARNING: RVV separable does not exactly match scalar separable.\n";
    }
}

int main() {
    const int width = 256;
    const int height = 256;
    const int iterations = 100;

    std::cout << "=== Separable Gaussian RVV Benchmark ===\n";
    std::cout << "Image size: " << width << "x" << height << "\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    Image input = createTestImage(width, height);

    Image scalar2DCheck = gaussianBlur(input);
    Image scalarSepCheck = gaussianBlurSeparable(input);
    Image rvvSepCheck = gaussianBlurSeparableRVV(input);

    std::cout << "Checksums before timing:\n";
    std::cout << "Scalar_2D_checksum: " << checksumImage(scalar2DCheck) << "\n";
    std::cout << "Scalar_separable_checksum: " << checksumImage(scalarSepCheck) << "\n";
    std::cout << "RVV_separable_checksum: " << checksumImage(rvvSepCheck) << "\n\n";

    compareImages(scalarSepCheck, rvvSepCheck);

    Image scalar2DOutput;
    Image scalarSepOutput;
    Image rvvSepOutput;

    uint64_t start;
    uint64_t end;

    start = readCycle();
    for (int i = 0; i < iterations; i++) {
        scalar2DOutput = gaussianBlur(input);
    }
    end = readCycle();
    double scalar2DCycles =
        static_cast<double>(end - start) / iterations;

    start = readCycle();
    for (int i = 0; i < iterations; i++) {
        scalarSepOutput = gaussianBlurSeparable(input);
    }
    end = readCycle();
    double scalarSepCycles =
        static_cast<double>(end - start) / iterations;

    start = readCycle();
    for (int i = 0; i < iterations; i++) {
        rvvSepOutput = gaussianBlurSeparableRVV(input);
    }
    end = readCycle();
    double rvvSepCycles =
        static_cast<double>(end - start) / iterations;

    std::cout << "Final checksums:\n";
    std::cout << "Scalar_2D_checksum: " << checksumImage(scalar2DOutput) << "\n";
    std::cout << "Scalar_separable_checksum: " << checksumImage(scalarSepOutput) << "\n";
    std::cout << "RVV_separable_checksum: " << checksumImage(rvvSepOutput) << "\n\n";

    std::cout << "Scalar_2D_Gaussian_cycles: " << scalar2DCycles << "\n";
    std::cout << "Scalar_Separable_Gaussian_cycles: " << scalarSepCycles << "\n";
    std::cout << "RVV_Separable_Gaussian_cycles: " << rvvSepCycles << "\n\n";

    std::cout << "Speedup_RVV_separable_vs_scalar_2D: "
              << scalar2DCycles / rvvSepCycles << "x\n";

    std::cout << "Speedup_RVV_separable_vs_scalar_separable: "
              << scalarSepCycles / rvvSepCycles << "x\n";

    return 0;
}
