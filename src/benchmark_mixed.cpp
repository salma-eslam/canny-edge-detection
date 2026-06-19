#include <iostream>

#include <string>

#include <stdexcept>

#include <time.h>

#include "types.h"

#include "image_io.h"

#include "gaussian_separable.h"   // gaussianBlurSeparable

#include "gradient.h"              // magnitudeL1Rvv, magnitudeL2

#include "direction.h"             // gradientDirection

#include "sobel.h"

// Converts the difference between two timespec values into milliseconds.

double elapsedMilliseconds(const timespec& startTime, const timespec& endTime) {

    double seconds = static_cast<double>(endTime.tv_sec - startTime.tv_sec);

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

        ./benchmark_optimized rect.raw 256 256

    */

    if (argc != 4) {

        std::cout << "Usage:\n";

        std::cout << "  " << argv[0] << " <input.raw> <width> <height>\n";

        std::cout << "Example:\n";

        std::cout << "  " << argv[0] << " rect.raw 256 256\n";

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

        timespec startTime;

        timespec endTime;



        /*

            Benchmark 1: Separable Gaussian blur.

            Two 1D passes (horizontal then vertical) — 10 MACs per pixel

            instead of 25 for the full 2D version.

        */

        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {

            Image blurredImage = gaussianBlurSeparable(originalImage);

        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);

        double gaussianSeparableMs = elapsedMilliseconds(startTime, endTime) / iterations;

        /*

            Prepare intermediate outputs once.

            These are used as inputs for later stage benchmarks.

        */

        Image blurredImage = gaussianBlurSeparable(originalImage);

        /*

            Benchmark 2: Fused RVV Sobel (Gx and Gy in a single pass).

            sobel_fused_rvv writes directly into the two output Image16

            buffers instead of returning them — signature differs from

            the scalar sobelX / sobelY pair.

        */

        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {

            Image16 gradientX, gradientY;

            sobel_fused_rvv(blurredImage, gradientX, gradientY);

        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);

        double sobelRvvMs = elapsedMilliseconds(startTime, endTime) / iterations;

        Image16 gradientX, gradientY;

        sobel_fused_rvv(blurredImage, gradientX, gradientY);

        /*

            Benchmark 3a: L1 magnitude — RVV version.

            Uses vector absolute value and widening add.

        */

        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {

            Image magnitudeL1Image = magnitudeL1Rvv(gradientX, gradientY);

        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);

        double magnitudeL1RvvMs = elapsedMilliseconds(startTime, endTime) / iterations;



        /*

            Benchmark 3b: L2 magnitude — scalar version.

            Uses sqrt(Gx^2 + Gy^2); kept scalar because RVV has no

            integer sqrt instruction, so a full RVV version gives

            minimal benefit over a well-optimised scalar loop here.

        */

        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {

            Image magnitudeL2Image = magnitudeL2(gradientX, gradientY);

        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);

        double magnitudeL2ScalarMs = elapsedMilliseconds(startTime, endTime) / iterations;

        /*

            Benchmark 4: Direction quantization — scalar version.

            Direction is ~8% of total runtime (Amdahl's law), so the

            overhead of RVV intrinsics would outweigh the gain here.

        */

        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {

            Image directionImage = gradientDirection(gradientX, gradientY);

        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);

        double directionScalarMs = elapsedMilliseconds(startTime, endTime) / iterations;

        /*

            Benchmark 5: Full optimised pipeline end-to-end.

            Gaussian separable  →  RVV Sobel  →  RVV L1 magnitude

            →  scalar direction.

            L1 magnitude is chosen for the full pipeline because it is

            the faster embedded-friendly magnitude method.

        */

        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {

            Image   blurred   = gaussianBlurSeparable(originalImage);

            Image16 gx, gy;

            sobel_fused_rvv(blurred, gx, gy);

            Image magnitude  = magnitudeL1Rvv(gx, gy);

            Image direction  = gradientDirection(gx, gy);

        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);

        double fullPipelineMs = elapsedMilliseconds(startTime, endTime) / iterations;



        std::cout << "Benchmark results averaged over "

                  << iterations << " iterations\n";

        std::cout << "Gaussian_Separable_Scalar_ms: " << gaussianSeparableMs   << "\n";

        std::cout << "Sobel_Fused_RVV_ms: "           << sobelRvvMs            << "\n";

        std::cout << "Magnitude_L1_RVV_ms: "          << magnitudeL1RvvMs      << "\n";

        std::cout << "Magnitude_L2_Scalar_ms: "       << magnitudeL2ScalarMs   << "\n";

        std::cout << "Direction_Scalar_ms: "          << directionScalarMs     << "\n";

        std::cout << "Full_Pipeline_ms: "             << fullPipelineMs        << "\n";

        return 0;

    }

    catch (const std::exception& error) {

        std::cerr << "Error: " << error.what() << "\n";

        return 1;

    }

}

