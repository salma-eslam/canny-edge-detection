#include <iostream>
#include <string>
#include <stdexcept>
#include <time.h>

#include "types.h"
#include "image_io.h"
#include "gaussian_separable.h"
#include "sobel.h"
#include "gradient.h"
#include "direction.h"

// RVV Sobel function from sobel_rvv.cpp
void sobel_fused_rvv(const Image& input, Image16& out_gx, Image16& out_gy);

static void printUsage(const char* programName) {
    std::cout << "Usage:\n";
    std::cout << "  " << programName
              << " <input.raw> <width> <height> <output_prefix>\n\n";

    std::cout << "Example:\n";
    std::cout << "  " << programName
              << " input/conan_512.raw 512 512 output/conan_sep_mixed\n\n";
}

static double elapsedMilliseconds(const timespec& startTime, const timespec& endTime) {
    double seconds =
        static_cast<double>(endTime.tv_sec - startTime.tv_sec);

    double nanoseconds =
        static_cast<double>(endTime.tv_nsec - startTime.tv_nsec);

    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        std::string inputFileName = argv[1];
        int imageWidth = std::stoi(argv[2]);
        int imageHeight = std::stoi(argv[3]);
        std::string outputFilePrefix = argv[4];

        if (imageWidth <= 0 || imageHeight <= 0) {
            std::cerr << "Error: image width and height must be positive numbers.\n";
            return 1;
        }

        std::cout << "=== Canny Edge Detection: Separable Gaussian + RVV Pipeline ===\n";
        std::cout << "Pipeline: Separable Gaussian scalar -> Sobel RVV -> Magnitude L1 RVV -> Magnitude L2 scalar -> Direction scalar\n\n";

        std::cout << "[1/6] Loading input image...\n";
        Image originalImage = loadRawImage(
            inputFileName,
            imageWidth,
            imageHeight
        );

        timespec pipelineStart;
        timespec pipelineEnd;

        clock_gettime(CLOCK_MONOTONIC, &pipelineStart);

        std::cout << "[2/6] Applying separable Gaussian blur...\n";
        Image blurredImage = gaussianBlurSeparable(originalImage);

        std::cout << "[3/6] Computing Sobel gradients (RVV fused)...\n";
        Image16 gradientX;
        Image16 gradientY;
        sobel_fused_rvv(blurredImage, gradientX, gradientY);

        std::cout << "[4/6] Computing L1 gradient magnitude (RVV)...\n";
        Image magnitudeL1Image = magnitudeL1Rvv(gradientX, gradientY);

        std::cout << "[5/6] Computing L2 gradient magnitude (scalar)...\n";
        Image magnitudeL2Image = magnitudeL2(gradientX, gradientY);

        std::cout << "[6/6] Computing gradient direction (scalar)...\n";
        Image directionImage = gradientDirection(gradientX, gradientY);

        clock_gettime(CLOCK_MONOTONIC, &pipelineEnd);

        double pipelineMilliseconds =
            elapsedMilliseconds(pipelineStart, pipelineEnd);

        std::cout << "\nSaving output files...\n";

        std::string blurOutputFile =
            outputFilePrefix + "_blur.raw";

        std::string magnitudeL1OutputFile =
            outputFilePrefix + "_magnitude_l1.raw";

        std::string magnitudeL2OutputFile =
            outputFilePrefix + "_magnitude_l2.raw";

        std::string directionOutputFile =
            outputFilePrefix + "_direction.raw";

        std::string edgesOutputFile =
            outputFilePrefix + "_edges.raw";

        saveRawImage(blurOutputFile, blurredImage);
        saveRawImage(magnitudeL1OutputFile, magnitudeL1Image);
        saveRawImage(magnitudeL2OutputFile, magnitudeL2Image);
        saveRawImage(directionOutputFile, directionImage);

        // Final visual output of the implemented pipeline.
        // Saved from L1 magnitude because it gives the clearest edge-strength image.
        saveRawImage(edgesOutputFile, magnitudeL1Image);

        std::cout << "\nPipeline completed successfully.\n";

        std::cout << "Generated files:\n";
        std::cout << "  " << blurOutputFile << "\n";
        std::cout << "  " << magnitudeL1OutputFile << "\n";
        std::cout << "  " << magnitudeL2OutputFile << "\n";
        std::cout << "  " << directionOutputFile << "\n";
        std::cout << "  " << edgesOutputFile << "  (final visual edge output)\n";

        std::cout << "\n=== Timing ===\n";
        std::cout << "Full pipeline time: "
                  << pipelineMilliseconds
                  << " ms\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}

