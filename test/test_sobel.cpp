#include <gtest/gtest.h>
#include "../src/sobel.h"
#include "../src/gradient.h"

// ─────────────────────────────────────────────
// Test 1: Uniform image should give zero gradient
// If all pixels have the same brightness, there
// are no edges, so Gx and Gy must be all zeros.
// Note: Test 3 builds on this same idea — if Gx
// and Gy are zero, magnitude must also be zero.
// ─────────────────────────────────────────────
TEST(SobelTest, UniformImageZeroGradient) {

    // Create a 5x5 image where every pixel = 100
    // This simulates a completely flat region with no edges
    Image input;
    input.width  = 5;
    input.height = 5;
    input.data.resize(25, 100);

    // Apply Sobel in both directions
    Image16 gx = sobelX(input);
    Image16 gy = sobelY(input);

    // Every value in gx and gy must be zero
    // because there are no brightness changes anywhere
    for (int i = 0; i < 25; i++) {
        EXPECT_EQ(gx.data[i], 0);
        EXPECT_EQ(gy.data[i], 0);
    }
}




// ─────────────────────────────────────────────
// Test 2: Vertical edge detection
// We create an image that is dark on the left
// and bright on the right. This creates a clear
// vertical edge in the middle of the image.
// sobelX should detect it (large nonzero value)
// sobelY should be zero (no up-down changes)
// ─────────────────────────────────────────────
TEST(SobelTest, VerticalEdgeDetection) {

    // Create a 5x5 image
    // Left 3 columns = dark (0), right 2 columns = bright (255)
    // This creates a vertical edge between column 2 and column 3
    Image input;
    input.width  = 5;
    input.height = 5;
    input.data.resize(25, 0);

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            if (x >= 3) {
                // Right side is bright
                input.data[y * 5 + x] = 255;
            }
        }
    }

    // Apply Sobel in both directions
    Image16 gx = sobelX(input);
    Image16 gy = sobelY(input);

    // sobelX must detect the vertical edge
    // The center pixel (row 2, col 2) should be nonzero
    EXPECT_NE(gx.data[2 * 5 + 2], 0);

    // sobelY must be zero everywhere
    // because brightness never changes from top to bottom
    for (int i = 0; i < 25; i++) {
        EXPECT_EQ(gy.data[i], 0);
    }
}






// ─────────────────────────────────────────────
// Test 3: Magnitude of uniform image is all zeros
// This test is connected to Test 1 — we use the
// same idea of a uniform image with no edges.
// If Gx and Gy are all zero (as proven in Test 1),
// then both magnitudeL1 and magnitudeL2 must also
// return all zeros because:
// L1 = |0| + |0| = 0
// L2 = sqrt(0² + 0²) = 0
// ─────────────────────────────────────────────
TEST(GradientTest, UniformImageZeroMagnitude) {

    // Create zero Gx and Gy
    // This simulates the output of sobelX and sobelY
    // on a completely uniform image (same as Test 1)
    Image16 gx, gy;
    gx.width  = gy.width  = 5;
    gx.height = gy.height = 5;
    gx.data.resize(25, 0);
    gy.data.resize(25, 0);

    // Compute both magnitude types
    Image magL1 = magnitudeL1(gx, gy);
    Image magL2 = magnitudeL2(gx, gy);

    // Both must return all zeros
    // because there are no edges in a uniform image
    for (int i = 0; i < 25; i++) {
        EXPECT_EQ(magL1.data[i], 0);
        EXPECT_EQ(magL2.data[i], 0);
    }
}

// Entry point — runs all tests above
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
