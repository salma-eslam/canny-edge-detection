#include "types.h"
#include "gaussian.h"
#include "gaussian_rvv.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cmath>

// readCycle
// Reads the RISC-V hardware cycle counter using the rdcycle instruction.
// Used for relative performance comparison between scalar and RVV versions.
// Note: QEMU is not cycle-accurate (spec: Measurement Methodology section),
// so absolute cycle counts are not meaningful in isolation, but relative
// comparisons (scalar vs RVV, LMUL=4 vs 2 vs 1) are valid since they
// reflect actual instruction count differences between implementations.
static inline uint64_t readCycle() {
    uint64_t cycles;
    asm volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

// createTestImage
// Generates a deterministic synthetic test image (no file I/O needed).
// Pattern is reproducible across runs for consistent benchmarking.
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

// compareImagesApprox
// Compares two images pixel-by-pixel and reports mismatch statistics.
// Used to verify RVV output matches scalar output within tolerance
// (spec section 3.2 allows +/-1 tolerance due to fixed-point rounding
// in the (sum*240)>>16 approximation of division by 273).
static void compareImagesApprox(const Image& scalar, const Image& rvv, const char* label) {
    int mismatchCount = 0;
    int maxDifference = 0;
    uint64_t totalDifference = 0;

    for (size_t i = 0; i < scalar.data.size(); i++) {
        int diff = std::abs(
            static_cast<int>(scalar.data[i]) -
            static_cast<int>(rvv.data[i])
        );
        if (diff != 0) {
            mismatchCount++;
        }
        if (diff > maxDifference) {
            maxDifference = diff;
        }
        totalDifference += static_cast<uint64_t>(diff);
    }

    double averageDifference =
        static_cast<double>(totalDifference) /
        static_cast<double>(scalar.data.size());

    std::cout << "--- Correctness vs scalar: " << label << " ---\n";
    std::cout << "Pixel_mismatches: " << mismatchCount << "\n";
    std::cout << "Max_pixel_difference: " << maxDifference << "\n";
    std::cout << "Average_pixel_difference: " << averageDifference << "\n\n";
}

// checksumImage
// Simple sum of all pixel values, used as a quick sanity-check fingerprint.
static uint64_t checksumImage(const Image& image) {
    uint64_t sum = 0;
    for (uint8_t pixel : image.data) {
        sum += pixel;
    }
    return sum;
}

int main() {
    const int width = 256;
    const int height = 256;
    const int iterations = 100;

    std::cout << "=== Gaussian Scalar vs RVV (LMUL=4, LMUL=2, LMUL=1) Benchmark ===\n";
    std::cout << "Image size: " << width << "x" << height << "\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    Image input = createTestImage(width, height);

    // --------------------------------------------------------
    // Correctness check (run once, not timed)
    // --------------------------------------------------------
    Image scalarCheck   = gaussianBlur(input);
    Image rvvLmul4Check = gaussianBlurRVV(input);
    Image rvvLmul2Check = gaussianBlurRVV_LMUL2(input);
    Image rvvLmul1Check = gaussianBlurRVV_LMUL1(input);

    compareImagesApprox(scalarCheck, rvvLmul4Check, "RVV LMUL=4");
    compareImagesApprox(scalarCheck, rvvLmul2Check, "RVV LMUL=2");
    compareImagesApprox(scalarCheck, rvvLmul1Check, "RVV LMUL=1");

    std::cout << "Scalar checksum:      " << checksumImage(scalarCheck)   << "\n";
    std::cout << "RVV LMUL=4 checksum:  " << checksumImage(rvvLmul4Check) << "\n";
    std::cout << "RVV LMUL=2 checksum:  " << checksumImage(rvvLmul2Check) << "\n";
    std::cout << "RVV LMUL=1 checksum:  " << checksumImage(rvvLmul1Check) << "\n\n";

    // --------------------------------------------------------
    // Warm-up iterations to populate caches / stabilize JIT behavior
    // --------------------------------------------------------
    volatile Image warmUpScalar = gaussianBlur(input);
    volatile Image warmUpLmul4  = gaussianBlurRVV(input);
    volatile Image warmUpLmul2  = gaussianBlurRVV_LMUL2(input);
    volatile Image warmUpLmul1  = gaussianBlurRVV_LMUL1(input);

    // --------------------------------------------------------
    // Timed runs: scalar baseline
    // --------------------------------------------------------
    uint64_t scalarStart = readCycle();
    for (int i = 0; i < iterations; i++) {
        volatile Image temp = gaussianBlur(input);
    }
    uint64_t scalarEnd = readCycle();

    // --------------------------------------------------------
    // Timed runs: RVV LMUL=4
    // --------------------------------------------------------
    uint64_t lmul4Start = readCycle();
    for (int i = 0; i < iterations; i++) {
        volatile Image temp = gaussianBlurRVV(input);
    }
    uint64_t lmul4End = readCycle();

    // --------------------------------------------------------
    // Timed runs: RVV LMUL=2
    // --------------------------------------------------------
    uint64_t lmul2Start = readCycle();
    for (int i = 0; i < iterations; i++) {
        volatile Image temp = gaussianBlurRVV_LMUL2(input);
    }
    uint64_t lmul2End = readCycle();

    // --------------------------------------------------------
    // Timed runs: RVV LMUL=1
    // --------------------------------------------------------
    uint64_t lmul1Start = readCycle();
    for (int i = 0; i < iterations; i++) {
        volatile Image temp = gaussianBlurRVV_LMUL1(input);
    }
    uint64_t lmul1End = readCycle();

    // --------------------------------------------------------
    // Compute and report results
    // --------------------------------------------------------
    double scalarCycles = static_cast<double>(scalarEnd - scalarStart) / iterations;
    double lmul4Cycles  = static_cast<double>(lmul4End  - lmul4Start)  / iterations;
    double lmul2Cycles  = static_cast<double>(lmul2End  - lmul2Start)  / iterations;
    double lmul1Cycles  = static_cast<double>(lmul1End  - lmul1Start)  / iterations;

    double speedupLmul4 = scalarCycles / lmul4Cycles;
    double speedupLmul2 = scalarCycles / lmul2Cycles;
    double speedupLmul1 = scalarCycles / lmul1Cycles;

    std::cout << "Scalar_Gaussian_cycles:     " << scalarCycles << "\n";
    std::cout << "RVV_LMUL4_Gaussian_cycles:  " << lmul4Cycles  << "\n";
    std::cout << "RVV_LMUL2_Gaussian_cycles:  " << lmul2Cycles  << "\n";
    std::cout << "RVV_LMUL1_Gaussian_cycles:  " << lmul1Cycles  << "\n\n";

    std::cout << "Speedup_LMUL4_vs_Scalar:    " << speedupLmul4 << "x\n";
    std::cout << "Speedup_LMUL2_vs_Scalar:    " << speedupLmul2 << "x\n";
    std::cout << "Speedup_LMUL1_vs_Scalar:    " << speedupLmul1 << "x\n";

    return 0;
}

