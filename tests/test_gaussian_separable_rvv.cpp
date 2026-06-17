#include "types.h"
#include "gaussian_separable.h"
#include "gaussian_separable_rvv.h"

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <string>

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

static bool compareExact(
    const Image& scalar,
    const Image& rvv,
    const std::string& testName
) {
    if (scalar.width != rvv.width || scalar.height != rvv.height) {
        std::cerr << "[FAIL] " << testName << ": dimension mismatch\n";
        return false;
    }

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

            if (mismatches <= 10) {
                std::cerr << "[MISMATCH] " << testName
                          << " index=" << i
                          << " scalar=" << static_cast<int>(scalar.data[i])
                          << " rvv=" << static_cast<int>(rvv.data[i])
                          << " diff=" << diff << "\n";
            }
        }

        if (diff > maxDiff) {
            maxDiff = diff;
        }

        totalDiff += static_cast<uint64_t>(diff);
    }

    double avgDiff =
        static_cast<double>(totalDiff) /
        static_cast<double>(scalar.data.size());

    std::cout << "[INFO] " << testName
              << " mismatches=" << mismatches
              << ", maxDiff=" << maxDiff
              << ", avgDiff=" << avgDiff << "\n";

    if (mismatches == 0) {
        std::cout << "[PASS] " << testName << "\n";
        return true;
    }

    std::cerr << "[FAIL] " << testName
              << ": RVV separable output does not exactly match scalar separable output\n";
    return false;
}

static bool runTest(int width, int height, const std::string& testName) {
    Image input = createTestImage(width, height);

    Image scalar = gaussianBlurSeparable(input);
    Image rvv = gaussianBlurSeparableRVV(input);

    return compareExact(scalar, rvv, testName);
}

int main() {
    bool allPassed = true;

    allPassed &= runTest(64, 64, "NormalSize_64x64");
    allPassed &= runTest(67, 53, "NonPowerOfTwo_67x53");
    allPassed &= runTest(7, 7, "SmallImage_7x7");

    if (allPassed) {
        std::cout << "\nAll separable RVV tests passed.\n";
        return 0;
    }

    std::cerr << "\nSome separable RVV tests failed.\n";
    return 1;
}
