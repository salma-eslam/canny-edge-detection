#include "types.h"
#include "gaussian.h"
#include "gaussian_rvv.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

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

static void compareImagesApprox(
    const Image& scalar,
    const Image& rvv,
    const char* label
) {
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

static uint64_t checksumImage(const Image& image) {
    uint64_t sum = 0;

    for (uint8_t pixel : image.data) {
        sum += pixel;
    }

    return sum;
}

int main() {
    const int width = 512;
    const int height = 512;
    const int iterations = 100;

    std::cout << "=== Gaussian Scalar vs RVV Benchmark ===\n";
    std::cout << "Image size: " << width << "x" << height << "\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    Image input = createTestImage(width, height);

    // --------------------------------------------------------
    // Correctness check
    // --------------------------------------------------------
    Image scalarCheck = gaussianBlur(input);

    Image rvvLmul4Check;
    Image rvvLmul2Check;
    Image rvvLmul1Check;

    gaussianBlurRVV(input, rvvLmul4Check);
    gaussianBlurRVV_LMUL2(input, rvvLmul2Check);
    gaussianBlurRVV_LMUL1(input, rvvLmul1Check);

    compareImagesApprox(scalarCheck, rvvLmul4Check, "RVV LMUL=4");
    compareImagesApprox(scalarCheck, rvvLmul2Check, "RVV LMUL=2");
    compareImagesApprox(scalarCheck, rvvLmul1Check, "RVV LMUL=1");

    std::cout << "Scalar checksum:      " << checksumImage(scalarCheck) << "\n";
    std::cout << "RVV LMUL=4 checksum:  " << checksumImage(rvvLmul4Check) << "\n";
    std::cout << "RVV LMUL=2 checksum:  " << checksumImage(rvvLmul2Check) << "\n";
    std::cout << "RVV LMUL=1 checksum:  " << checksumImage(rvvLmul1Check) << "\n\n";

    // --------------------------------------------------------
    // Pre-allocated output buffers for timed RVV runs
    // This avoids heap allocation inside the benchmark loop.
    // --------------------------------------------------------
    Image outputLmul4;
    Image outputLmul2;
    Image outputLmul1;

    outputLmul4.width = width;
    outputLmul4.height = height;
    outputLmul4.data.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    outputLmul2.width = width;
    outputLmul2.height = height;
    outputLmul2.data.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    outputLmul1.width = width;
    outputLmul1.height = height;
    outputLmul1.data.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    // --------------------------------------------------------
    // Warm-up
    // --------------------------------------------------------
    volatile uint64_t warmupChecksum = 0;

    Image warmUpScalar = gaussianBlur(input);
    warmupChecksum += checksumImage(warmUpScalar);

    gaussianBlurRVV(input, outputLmul4);
    warmupChecksum += checksumImage(outputLmul4);

    gaussianBlurRVV_LMUL2(input, outputLmul2);
    warmupChecksum += checksumImage(outputLmul2);

    gaussianBlurRVV_LMUL1(input, outputLmul1);
    warmupChecksum += checksumImage(outputLmul1);

    // --------------------------------------------------------
    // Timed scalar baseline
    // --------------------------------------------------------
    uint64_t scalarChecksum = 0;

    uint64_t scalarStart = readCycle();

    for (int i = 0; i < iterations; i++) {
        Image temp = gaussianBlur(input);
        scalarChecksum += temp.data[static_cast<size_t>(i) % temp.data.size()];
    }

    uint64_t scalarEnd = readCycle();

    // --------------------------------------------------------
    // Timed RVV LMUL=4
    // --------------------------------------------------------
    uint64_t lmul4Checksum = 0;

    uint64_t lmul4Start = readCycle();

    for (int i = 0; i < iterations; i++) {
        gaussianBlurRVV(input, outputLmul4);
        lmul4Checksum += outputLmul4.data[static_cast<size_t>(i) % outputLmul4.data.size()];
    }

    uint64_t lmul4End = readCycle();

    // --------------------------------------------------------
    // Timed RVV LMUL=2
    // --------------------------------------------------------
    uint64_t lmul2Checksum = 0;

    uint64_t lmul2Start = readCycle();

    for (int i = 0; i < iterations; i++) {
        gaussianBlurRVV_LMUL2(input, outputLmul2);
        lmul2Checksum += outputLmul2.data[static_cast<size_t>(i) % outputLmul2.data.size()];
    }

    uint64_t lmul2End = readCycle();

    // --------------------------------------------------------
    // Timed RVV LMUL=1
    // --------------------------------------------------------
    uint64_t lmul1Checksum = 0;

    uint64_t lmul1Start = readCycle();

    for (int i = 0; i < iterations; i++) {
        gaussianBlurRVV_LMUL1(input, outputLmul1);
        lmul1Checksum += outputLmul1.data[static_cast<size_t>(i) % outputLmul1.data.size()];
    }

    uint64_t lmul1End = readCycle();

    // Prevent compiler from removing benchmarked work.
    volatile uint64_t preventOptimization =
        warmupChecksum +
        scalarChecksum +
        lmul4Checksum +
        lmul2Checksum +
        lmul1Checksum;

    (void)preventOptimization;

    // --------------------------------------------------------
    // Results
    // --------------------------------------------------------
    double scalarCycles =
        static_cast<double>(scalarEnd - scalarStart) / iterations;

    double lmul4Cycles =
        static_cast<double>(lmul4End - lmul4Start) / iterations;

    double lmul2Cycles =
        static_cast<double>(lmul2End - lmul2Start) / iterations;

    double lmul1Cycles =
        static_cast<double>(lmul1End - lmul1Start) / iterations;

    double speedupLmul4 = scalarCycles / lmul4Cycles;
    double speedupLmul2 = scalarCycles / lmul2Cycles;
    double speedupLmul1 = scalarCycles / lmul1Cycles;

    std::cout << "Scalar_Gaussian_cycles:     " << scalarCycles << "\n";
    std::cout << "RVV_LMUL4_Gaussian_cycles:  " << lmul4Cycles << "\n";
    std::cout << "RVV_LMUL2_Gaussian_cycles:  " << lmul2Cycles << "\n";
    std::cout << "RVV_LMUL1_Gaussian_cycles:  " << lmul1Cycles << "\n\n";

    std::cout << "Speedup_LMUL4_vs_Scalar:    " << speedupLmul4 << "x\n";
    std::cout << "Speedup_LMUL2_vs_Scalar:    " << speedupLmul2 << "x\n";
    std::cout << "Speedup_LMUL1_vs_Scalar:    " << speedupLmul1 << "x\n\n";

    std::cout << "Benchmark guard checksum:   " << preventOptimization << "\n";

    return 0;
}
