#include <iostream>
#include <string>
#include <stdexcept>
#include <time.h>

#include "types.h"
#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "gradient.h"
#include "direction.h"

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
        ./benchmark_O2 rect.raw 256 256
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

        /*
            Benchmark 1: Gaussian blur only.
            This tells us how expensive the smoothing stage is.
        */
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {
            Image blurredImage = gaussianBlur(originalImage);
        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double gaussianMs = elapsedMilliseconds(startTime, endTime) / iterations;

        /*
            Prepare intermediate outputs once.
            These are used as input for later stage benchmarks.
        */
        Image blurredImage = gaussianBlur(originalImage);

        /*
            Benchmark 2: Sobel X and Sobel Y.
        */
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {
            Image16 gradientX = sobelX(blurredImage);
            Image16 gradientY = sobelY(blurredImage);
        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double sobelMs = elapsedMilliseconds(startTime, endTime) / iterations;

        Image16 gradientX = sobelX(blurredImage);
        Image16 gradientY = sobelY(blurredImage);

        /*
            Benchmark 3: Magnitude L1 and L2 together.
            We time them together because your current scalar pipeline computes both.
        */
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {
            Image magnitudeL1Image = magnitudeL1(gradientX, gradientY);
            Image magnitudeL2Image = magnitudeL2(gradientX, gradientY);
        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double magnitudeMs = elapsedMilliseconds(startTime, endTime) / iterations;

        /*
            Benchmark 4: Direction quantization.
        */
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {
            Image directionImage = gradientDirection(gradientX, gradientY);
        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double directionMs = elapsedMilliseconds(startTime, endTime) / iterations;

        /*
            Benchmark 5: Full pipeline.
            This measures the complete scalar pipeline from input image to final outputs.
            Here we use L1 magnitude only inside the full pipeline because L1 is the faster
            embedded-friendly magnitude method.
        */
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        for (int i = 0; i < iterations; i++) {
            Image blurred = gaussianBlur(originalImage);
            Image16 gx = sobelX(blurred);
            Image16 gy = sobelY(blurred);
            Image magnitude = magnitudeL1(gx, gy);
            Image direction = gradientDirection(gx, gy);
        }

        clock_gettime(CLOCK_MONOTONIC, &endTime);
        double fullPipelineMs = elapsedMilliseconds(startTime, endTime) / iterations;

        std::cout << "Benchmark results averaged over "
                  << iterations << " iterations\n";

        std::cout << "Gaussian_ms: " << gaussianMs << "\n";
        std::cout << "Sobel_ms: " << sobelMs << "\n";
        std::cout << "Magnitude_L1_L2_ms: " << magnitudeMs << "\n";
        std::cout << "Direction_ms: " << directionMs << "\n";
        std::cout << "Full_pipeline_ms: " << fullPipelineMs << "\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
