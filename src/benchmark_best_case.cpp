#include <iostream>
#include <string>
#include <stdexcept>
#include <time.h>
#include "clock_shim.h"
#include <cmath>

#include "types.h"
#include "image_io.h"

// Include original project headers
#include "gaussian.h"
#include "sobel.h"
#include "gradient.h"
#include "direction.h"

#ifdef __riscv
#include <riscv_vector.h>
#endif

// Converts the difference between two timespec values into milliseconds.
double elapsedMilliseconds(const timespec& startTime, const timespec& endTime) {
    double seconds = static_cast<double>(endTime.tv_sec - startTime.tv_sec);
    double nanoseconds = static_cast<double>(endTime.tv_nsec - startTime.tv_nsec);
    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " <input.raw> <width> <height>\n";
        return 1;
    }

    try {
        std::string inputFileName = argv[1];
        int imageWidth = std::stoi(argv[2]);
        int imageHeight = std::stoi(argv[3]);

        if (imageWidth <= 0 || imageHeight <= 0) {
            std::cerr << "Error: width and height must be positive.\n";
            return 1;
        }

        const int iterations = 100;
        Image originalImage = loadRawImage(inputFileName, imageWidth, imageHeight);

        timespec startTime;
        timespec endTime;

        // --------------------------------------------------------------------
        // BENCHMARK 1: Gaussian Blur Only (Currently Scalar Baseline)
        // --------------------------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image blurredImage = gaussianBlur(originalImage);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double gaussianMs = elapsedMilliseconds(startTime, endTime) / iterations;

        Image blurredImage = gaussianBlur(originalImage);

        // --------------------------------------------------------------------
        // BENCHMARK 2: Sobel Operator Only (Best Case: SCALAR)
        // --------------------------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image16 gradientX = sobelX(blurredImage);
            Image16 gradientY = sobelY(blurredImage);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double sobelMs = elapsedMilliseconds(startTime, endTime) / iterations;

        Image16 gradientX = sobelX(blurredImage);
        Image16 gradientY = sobelY(blurredImage);

        // --------------------------------------------------------------------
        // BENCHMARK 3: Magnitude Separate Isolation Sweeps
        // --------------------------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image magnitudeL1Image = magnitudeL1Rvv(gradientX, gradientY);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double magnitudeL1Ms = elapsedMilliseconds(startTime, endTime) / iterations;

        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image magnitudeL2Image = magnitudeL2(gradientX, gradientY); // Best Case: Scalar
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double magnitudeL2Ms = elapsedMilliseconds(startTime, endTime) / iterations;

        // --------------------------------------------------------------------
        // BENCHMARK 4: Direction Quantization (Scalar)
        // --------------------------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image directionImage = gradientDirection(gradientX, gradientY);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double directionMs = elapsedMilliseconds(startTime, endTime) / iterations;

        // --------------------------------------------------------------------
        // BENCHMARK 5: Complete Hybrid Custom Pipeline (The Master Configuration)
        // --------------------------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image blurred = gaussianBlur(originalImage);
            Image16 gx = sobelX(blurred);
            Image16 gy = sobelY(blurred);
            Image magnitude = magnitudeL1Rvv(gx, gy); // Best Case: Your RVV
            Image direction = gradientDirection(gx, gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double hybridPipelineMs = elapsedMilliseconds(startTime, endTime) / iterations;

        // Print final clear results
        std::cout << "========================================\n";
        std::cout << "  BEST CASE HYBRID PIPELINE PROFILE     \n";
        std::cout << "========================================\n";
        std::cout << "Averaged over " << iterations << " iterations\n\n";
        std::cout << "Gaussian (Scalar)_ms:      " << gaussianMs << "\n";
        std::cout << "Sobel (Scalar)_ms:         " << sobelMs << "\n";
        std::cout << "Magnitude_L1 (RVV)_ms:     " << magnitudeL1Ms << "\n";
        std::cout << "Magnitude_L2 (Scalar)_ms:  " << magnitudeL2Ms << "\n";
        std::cout << "Direction (Scalar)_ms:     " << directionMs << "\n";
        std::cout << "----------------------------------------\n";
        std::cout << "Total Hybrid Pipeline_ms:  " << hybridPipelineMs << "\n";
        std::cout << "========================================\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
