#include <iostream>
#include <string>
#include <stdexcept>
#include <time.h>
#include "clock_shim.h"

#include "types.h"
#include "image_io.h"
#include "gaussian_separable.h"       // gaussianBlurSeparable        (scalar sep)
#include "gaussian_separable_rvv.h"   // gaussianBlurSeparableRVV     (RVV sep)
#include "gaussian.h"                 // gaussianBlur                 (scalar 2D)
#include "gaussian_rvv.h"             // gaussianBlurRVV              (RVV 2D)
#include "sobel.h"                    // sobelX, sobelY               (scalar)
                                      // sobel_fused_rvv              (RVV, same header)
#include "gradient.h"                 // magnitudeL1, magnitudeL2     (scalar)
                                      // magnitudeL1Rvv, magnitudeL2Rvv (RVV, same header)
#include "direction.h"                // gradientDirection            (scalar only)

double elapsedMilliseconds(const timespec& startTime, const timespec& endTime) {
    double seconds     = static_cast<double>(endTime.tv_sec  - startTime.tv_sec);
    double nanoseconds = static_cast<double>(endTime.tv_nsec - startTime.tv_nsec);
    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

int main(int argc, char* argv[]) {
    /*
        Expected arguments:
        argv[1] = input raw image file
        argv[2] = image width
        argv[3] = image height

        Example:
        ./benchmark_scenarios rect.raw 512 512
    */
    if (argc != 4) {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " <input.raw> <width> <height>\n";
        std::cout << "Example:\n";
        std::cout << "  " << argv[0] << " rect.raw 512 512\n";
        return 1;
    }

    try {
        std::string inputFileName = argv[1];
        int imageWidth  = std::stoi(argv[2]);
        int imageHeight = std::stoi(argv[3]);

        if (imageWidth <= 0 || imageHeight <= 0) {
            std::cerr << "Error: width and height must be positive.\n";
            return 1;
        }

        const int iterations = 100;
        Image originalImage = loadRawImage(inputFileName, imageWidth, imageHeight);

        timespec startTime, endTime;

        // ════════════════════════════════════════════════════════════
        // SCENARIO 1: Gaussian Sep RVV + Sobel RVV + Mag L1 RVV
        //             + Mag L2 Scalar + Direction Scalar
        // ════════════════════════════════════════════════════════════
        std::cout << "=== Scenario 1: Gaussian RVV + Sobel RVV + Mag L1 RVV + Mag L2 Scalar + Dir Scalar ===\n";

        // Gaussian Separable RVV
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image b = gaussianBlurSeparableRVV(originalImage);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s1_gaussian = elapsedMilliseconds(startTime, endTime) / iterations;

        Image s1_blurred = gaussianBlurSeparableRVV(originalImage);

        // Sobel Fused RVV
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image16 gx, gy;
            sobel_fused_rvv(s1_blurred, gx, gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s1_sobel = elapsedMilliseconds(startTime, endTime) / iterations;

        Image16 s1_gx, s1_gy;
        sobel_fused_rvv(s1_blurred, s1_gx, s1_gy);

        // Magnitude L1 RVV
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image m = magnitudeL1Rvv(s1_gx, s1_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s1_magL1 = elapsedMilliseconds(startTime, endTime) / iterations;

        // Magnitude L2 Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image m = magnitudeL2(s1_gx, s1_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s1_magL2 = elapsedMilliseconds(startTime, endTime) / iterations;

        // Direction Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image d = gradientDirection(s1_gx, s1_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s1_dir = elapsedMilliseconds(startTime, endTime) / iterations;

        // Full pipeline scenario 1
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image   blurred = gaussianBlurSeparableRVV(originalImage);
            Image16 gx, gy;
            sobel_fused_rvv(blurred, gx, gy);
            Image   mag = magnitudeL1Rvv(gx, gy);
            Image   dir = gradientDirection(gx, gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s1_full = elapsedMilliseconds(startTime, endTime) / iterations;

        std::cout << "Gaussian_Separable_RVV_ms: "  << s1_gaussian << "\n";
        std::cout << "Sobel_Fused_RVV_ms: "          << s1_sobel   << "\n";
        std::cout << "Magnitude_L1_RVV_ms: "         << s1_magL1   << "\n";
        std::cout << "Magnitude_L2_Scalar_ms: "      << s1_magL2   << "\n";
        std::cout << "Direction_Scalar_ms: "         << s1_dir     << "\n";
        std::cout << "Full_Pipeline_ms: "            << s1_full    << "\n\n";

        // ════════════════════════════════════════════════════════════
        // SCENARIO 2: All Scalar
        //             Gaussian Sep Scalar + sobelX/sobelY + Mag L1
        //             + Mag L2 + Direction
        // ════════════════════════════════════════════════════════════
        std::cout << "=== Scenario 2: All Scalar (2D Gaussian) ===\n";

        // Gaussian 2D Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image b = gaussianBlur(originalImage);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s2_gaussian = elapsedMilliseconds(startTime, endTime) / iterations;

        Image s2_blurred = gaussianBlur(originalImage);

        // Sobel Scalar (sobelX + sobelY as separate calls)
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image16 gx = sobelX(s2_blurred);
            Image16 gy = sobelY(s2_blurred);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s2_sobel = elapsedMilliseconds(startTime, endTime) / iterations;

        Image16 s2_gx = sobelX(s2_blurred);
        Image16 s2_gy = sobelY(s2_blurred);

        // Magnitude L1 Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image m = magnitudeL1(s2_gx, s2_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s2_magL1 = elapsedMilliseconds(startTime, endTime) / iterations;

        // Magnitude L2 Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image m = magnitudeL2(s2_gx, s2_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s2_magL2 = elapsedMilliseconds(startTime, endTime) / iterations;

        // Direction Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image d = gradientDirection(s2_gx, s2_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s2_dir = elapsedMilliseconds(startTime, endTime) / iterations;

        // Full pipeline scenario 2
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image   blurred = gaussianBlur(originalImage);
            Image16 gx      = sobelX(blurred);
            Image16 gy      = sobelY(blurred);
            Image   mag     = magnitudeL1(gx, gy);
            Image   dir     = gradientDirection(gx, gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s2_full = elapsedMilliseconds(startTime, endTime) / iterations;

        std::cout << "Gaussian_2D_Scalar_ms: "        << s2_gaussian << "\n";
        std::cout << "Sobel_Scalar_ms: "               << s2_sobel   << "\n";
        std::cout << "Magnitude_L1_Scalar_ms: "        << s2_magL1   << "\n";
        std::cout << "Magnitude_L2_Scalar_ms: "        << s2_magL2   << "\n";
        std::cout << "Direction_Scalar_ms: "           << s2_dir     << "\n";
        std::cout << "Full_Pipeline_ms: "              << s2_full    << "\n\n";

        // ════════════════════════════════════════════════════════════
        // SCENARIO 3: All RVV
        //             Gaussian Sep RVV + Sobel RVV + Mag L1 RVV
        //             + Mag L2 RVV + Direction Scalar
        //             (Direction has no RVV version — scalar kept,
        //              it is ~3% of runtime so gain would be minimal)
        // ════════════════════════════════════════════════════════════
        std::cout << "=== Scenario 3: All RVV (2D Gaussian RVV, Direction scalar — no RVV version) ===\n";

        // Gaussian 2D RVV (LMUL=4 default)
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image b = gaussianBlurRVV(originalImage);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s3_gaussian = elapsedMilliseconds(startTime, endTime) / iterations;

        Image s3_blurred = gaussianBlurRVV(originalImage);

        // Sobel Fused RVV
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image16 gx, gy;
            sobel_fused_rvv(s3_blurred, gx, gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s3_sobel = elapsedMilliseconds(startTime, endTime) / iterations;

        Image16 s3_gx, s3_gy;
        sobel_fused_rvv(s3_blurred, s3_gx, s3_gy);

        // Magnitude L1 RVV
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image m = magnitudeL1Rvv(s3_gx, s3_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s3_magL1 = elapsedMilliseconds(startTime, endTime) / iterations;

        // Magnitude L2 RVV
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image m = magnitudeL2Rvv(s3_gx, s3_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s3_magL2 = elapsedMilliseconds(startTime, endTime) / iterations;

        // Direction Scalar
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image d = gradientDirection(s3_gx, s3_gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s3_dir = elapsedMilliseconds(startTime, endTime) / iterations;

        // Full pipeline scenario 3
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        for (int i = 0; i < iterations; i++) {
            Image   blurred = gaussianBlurRVV(originalImage);
            Image16 gx, gy;
            sobel_fused_rvv(blurred, gx, gy);
            Image   mag = magnitudeL1Rvv(gx, gy);
            Image   dir = gradientDirection(gx, gy);
        }
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double s3_full = elapsedMilliseconds(startTime, endTime) / iterations;

        std::cout << "Gaussian_2D_RVV_ms: "        << s3_gaussian << "\n";
        std::cout << "Sobel_Fused_RVV_ms: "         << s3_sobel   << "\n";
        std::cout << "Magnitude_L1_RVV_ms: "        << s3_magL1   << "\n";
        std::cout << "Magnitude_L2_RVV_ms: "        << s3_magL2   << "\n";
        std::cout << "Direction_Scalar_ms: "        << s3_dir     << "\n";
        std::cout << "Full_Pipeline_ms: "           << s3_full    << "\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
