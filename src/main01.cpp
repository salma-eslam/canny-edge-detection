#include <iostream>
#include <string>
#include <stdexcept>
#include <time.h>

#include "types.h"
#include "image_io.h"
#include "gaussian_rvv.h"
#include "sobel.h"
#include "gradient.h"
#include "direction.h"

// RVV Sobel function implemented in sobel_rvv.cpp
void sobel_fused_rvv(const Image& input, Image16& out_gx, Image16& out_gy);

// Prints how the user should run the program.
// This is shown when the user gives the wrong number of arguments.
static void printUsage(const char* programName) {
    std::cout << "Usage:\n";
    std::cout << "  " << programName
              << " <input.raw> <width> <height> <output_prefix>\n\n";

    std::cout << "Example:\n";
    std::cout << "  " << programName
              << " input/ferrari_512.raw 512 512 output/ferrari_rvv\n\n";

    std::cout << "This means:\n";
    std::cout << "  input.raw      = raw grayscale input image\n";
    std::cout << "  width          = image width in pixels\n";
    std::cout << "  height         = image height in pixels\n";
    std::cout << "  output_prefix  = prefix used for all saved output files\n";
}

// Returns elapsed time in milliseconds.
static double elapsedMilliseconds(const timespec& startTime, const timespec& endTime) {
    double seconds =
        static_cast<double>(endTime.tv_sec - startTime.tv_sec);

    double nanoseconds =
        static_cast<double>(endTime.tv_nsec - startTime.tv_nsec);

    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

int main(int argc, char* argv[]) {
    /*
        Expected command-line arguments:

        argv[0] = program name
        argv[1] = input raw image file path
        argv[2] = image width
        argv[3] = image height
        argv[4] = output file prefix

        So argc must be 5.
    */
    if (argc != 5) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        // Read command-line arguments.
        std::string inputFileName = argv[1];
        int imageWidth = std::stoi(argv[2]);
        int imageHeight = std::stoi(argv[3]);
        std::string outputFilePrefix = argv[4];

        // Basic validation to avoid invalid image sizes.
        if (imageWidth <= 0 || imageHeight <= 0) {
            std::cerr << "Error: image width and height must be positive numbers.\n";
            return 1;
        }

        std::cout << "=== Canny Edge Detection RVV Pipeline ===\n";
        std::cout << "Pipeline: Gaussian RVV -> Sobel RVV -> Magnitude L1 RVV -> Magnitude L2 scalar -> Direction scalar\n\n";

        /*
            Step 1: Load raw grayscale image.

            The input file is expected to contain exactly:
                imageWidth * imageHeight bytes

            Each byte represents one grayscale pixel:
                0   = black
                255 = white
        */
        std::cout << "[1/6] Loading input image...\n";
        Image originalImage = loadRawImage(
            inputFileName,
            imageWidth,
            imageHeight
        );

        /*
            Start full pipeline timing after loading the input image.
            This keeps file I/O separate from the actual image-processing time.
        */
        timespec pipelineStart;
        timespec pipelineEnd;

        clock_gettime(CLOCK_MONOTONIC, &pipelineStart);

        /*
            Step 2: Apply Gaussian blur using RVV.

            This should match the scalar Gaussian stage in purpose:
            smoothing the image before Sobel edge detection.
        */
        std::cout << "[2/6] Applying Gaussian blur (RVV)...\n";
        Image blurredImage = gaussianBlurRVV(originalImage);

        /*
            Step 3: Compute Sobel gradients using RVV.

            sobel_fused_rvv computes both Gx and Gy in one pass.
            The outputs are Image16 because Sobel values can be negative
            and can exceed the 0-255 range.
        */
        std::cout << "[3/6] Computing Sobel gradients (RVV fused)...\n";
        Image16 gradientX;
        Image16 gradientY;
        sobel_fused_rvv(blurredImage, gradientX, gradientY);

        /*
            Step 4: Compute L1 gradient magnitude using RVV.

            L1 magnitude formula:
                |Gx| + |Gy|

            This is also used as the final visual edge output,
            exactly like the scalar main version.
        */
        std::cout << "[4/6] Computing L1 gradient magnitude (RVV)...\n";
        Image magnitudeL1Image = magnitudeL1Rvv(gradientX, gradientY);

        /*
            Step 5: Compute L2 gradient magnitude using scalar.

            L2 magnitude formula:
                sqrt(Gx^2 + Gy^2)
        */
        std::cout << "[5/6] Computing L2 gradient magnitude (scalar)...\n";
        Image magnitudeL2Image = magnitudeL2(gradientX, gradientY);

        /*
            Step 6: Quantize gradient direction using scalar.

            We only classify the direction into 4 categories:

                0 = 0 degrees
                1 = 45 degrees
                2 = 90 degrees
                3 = 135 degrees
        */
        std::cout << "[6/6] Computing gradient direction (scalar)...\n";
        Image directionImage = gradientDirection(gradientX, gradientY);

        clock_gettime(CLOCK_MONOTONIC, &pipelineEnd);

        double pipelineMilliseconds =
            elapsedMilliseconds(pipelineStart, pipelineEnd);

        /*
            Save output images.

            Same output logic as main.cpp:
            - blur image: confirms Gaussian blur works
            - magnitude images: show detected edges
            - direction image: shows quantized edge direction values
            - edges image: same as L1 magnitude, saved as the final visual edge output

            No threshold is applied here, so the output should look like
            the original good edge image.
        */
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

        // Final output photo: same logic as scalar main.
        // This is the visual edge map of the implemented pipeline.
        saveRawImage(edgesOutputFile, magnitudeL1Image);

        std::cout << "\nPipeline completed successfully.\n";
        std::cout << "Generated files:\n";
        std::cout << "  " << blurOutputFile << "\n";
        std::cout << "  " << magnitudeL1OutputFile << "\n";
        std::cout << "  " << magnitudeL2OutputFile << "\n";
        std::cout << "  " << directionOutputFile << "\n";
        std::cout << "  " << edgesOutputFile << "  (final edge output, same as L1 magnitude)\n";

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
