#include "types.h"
#include "gaussian_separable.h"
#include "gaussian_separable_rvv.h"
#include "gaussian_rvv.h"

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cmath>

static inline uint64_t readCycle() {
    uint64_t cycles;
    asm volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

static Image createTestImage(int width, int height) {
    Image input;
    input.width = width;
    input.height = height;
    input.data.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            input.data[y * width + x] =
                static_cast<uint8_t>((x * 3 + y * 5) % 256);
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

static void compareImages(
    const Image& reference,
    const Image& test,
    const char* label
) {
    int mismatches = 0;
    int maxDiff = 0;
    uint64_t totalDiff = 0;

    for (size_t i = 0; i < reference.data.size(); i++) {
        int diff = std::abs(
            static_cast<int>(reference.data[i]) -
            static_cast<int>(test.data[i])
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
        static_cast<double>(reference.data.size());

    std::cout << "--- Correctness check: " << label << " ---\n";
    std::cout << "Pixel_mismatches: " << mismatches << "\n";
    std::cout << "Max_pixel_difference: " << maxDiff << "\n";
    std::cout << "Average_pixel_difference: " << avgDiff << "\n\n";
}

int main() {
    const int width = 512;
    const int height = 512;
    const int iterations = 100;

    std::cout << "=== Gaussian Comparison Benchmark ===\n";
    std::cout << "Compared kernels:\n";
    std::cout << "1. Scalar separable Gaussian\n";
    std::cout << "2. RVV separable Gaussian\n";
    std::cout << "3. RVV 2D Gaussian\n";
    std::cout << "Image size: " << width << "x" << height << "\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    Image input = createTestImage(width, height);

    // --------------------------------------------------------
    // Correctness / checksum checks before timing
    // --------------------------------------------------------
    Image scalarSeparableCheck = gaussianBlurSeparable(input);
    Image rvvSeparableCheck = gaussianBlurSeparableRVV(input);

    Image rvv2DCheck;
    gaussianBlurRVV(input, rvv2DCheck);

    std::cout << "Checksums before timing:\n";
    std::cout << "Scalar_Separable_checksum: " << checksumImage(scalarSeparableCheck) << "\n";
    std::cout << "RVV_Separable_checksum:    " << checksumImage(rvvSeparableCheck) << "\n";
    std::cout << "RVV_2D_Gaussian_checksum:  " << checksumImage(rvv2DCheck) << "\n\n";

    compareImages(
        scalarSeparableCheck,
        rvvSeparableCheck,
        "RVV separable vs scalar separable"
    );

    compareImages(
        scalarSeparableCheck,
        rvv2DCheck,
        "RVV 2D Gaussian vs scalar separable"
    );

    // --------------------------------------------------------
    // Warm-up
    // --------------------------------------------------------
    volatile uint64_t warmupChecksum = 0;

    Image warmupScalarSeparable = gaussianBlurSeparable(input);
    warmupChecksum += checksumImage(warmupScalarSeparable);

    Image warmupRVVSeparable = gaussianBlurSeparableRVV(input);
    warmupChecksum += checksumImage(warmupRVVSeparable);

    Image warmupRVV2D;
    gaussianBlurRVV(input, warmupRVV2D);
    warmupChecksum += checksumImage(warmupRVV2D);

    // --------------------------------------------------------
    // Timed scalar separable Gaussian
    // --------------------------------------------------------
    uint64_t scalarSeparableGuard = 0;

    uint64_t scalarSeparableStart = readCycle();

    for (int i = 0; i < iterations; i++) {
        Image output = gaussianBlurSeparable(input);
        scalarSeparableGuard +=
            output.data[static_cast<size_t>(i) % output.data.size()];
    }

    uint64_t scalarSeparableEnd = readCycle();

    // --------------------------------------------------------
    // Timed RVV separable Gaussian
    // --------------------------------------------------------
    uint64_t rvvSeparableGuard = 0;

    uint64_t rvvSeparableStart = readCycle();

    for (int i = 0; i < iterations; i++) {
        Image output = gaussianBlurSeparableRVV(input);
        rvvSeparableGuard +=
            output.data[static_cast<size_t>(i) % output.data.size()];
    }

    uint64_t rvvSeparableEnd = readCycle();

    // --------------------------------------------------------
    // Timed RVV 2D Gaussian
    // --------------------------------------------------------
    uint64_t rvv2DGuard = 0;

    Image rvv2DOutput;
    rvv2DOutput.width = width;
    rvv2DOutput.height = height;
    rvv2DOutput.data.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    uint64_t rvv2DStart = readCycle();

    for (int i = 0; i < iterations; i++) {
        gaussianBlurRVV(input, rvv2DOutput);
        rvv2DGuard +=
            rvv2DOutput.data[static_cast<size_t>(i) % rvv2DOutput.data.size()];
    }

    uint64_t rvv2DEnd = readCycle();

    volatile uint64_t preventOptimization =
        warmupChecksum +
        scalarSeparableGuard +
        rvvSeparableGuard +
        rvv2DGuard;

    (void)preventOptimization;

    // --------------------------------------------------------
    // Results
    // --------------------------------------------------------
    double scalarSeparableCycles =
        static_cast<double>(scalarSeparableEnd - scalarSeparableStart) /
        iterations;

    double rvvSeparableCycles =
        static_cast<double>(rvvSeparableEnd - rvvSeparableStart) /
        iterations;

    double rvv2DCycles =
        static_cast<double>(rvv2DEnd - rvv2DStart) /
        iterations;

    std::cout << "=== Timing Results ===\n";
    std::cout << "Scalar_Separable_Gaussian_cycles: " << scalarSeparableCycles << "\n";
    std::cout << "RVV_Separable_Gaussian_cycles:    " << rvvSeparableCycles << "\n";
    std::cout << "RVV_2D_Gaussian_cycles:           " << rvv2DCycles << "\n\n";

    std::cout << "Speedup_RVV_Separable_vs_Scalar_Separable: "
              << scalarSeparableCycles / rvvSeparableCycles << "x\n";

    std::cout << "Speedup_RVV_2D_vs_Scalar_Separable: "
              << scalarSeparableCycles / rvv2DCycles << "x\n";

    std::cout << "Speedup_RVV_Separable_vs_RVV_2D: "
              << rvv2DCycles / rvvSeparableCycles << "x\n\n";

    std::cout << "Benchmark guard checksum: " << preventOptimization << "\n";

    return 0;
}

