#include <gtest/gtest.h>
#include "image_io.h"
#include "gaussian_separable.h"

// Test 1: Uniform image stays uniform
// Same logic as the 2D Gaussian test — since every pixel is equal,
// every weighted sum equals value*17 per pass, divided by 17 = value.
TEST(SeparableGaussianTest, UniformImageStaysUniform) {
    int W = 64, H = 64;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 128);
    Image output = gaussianBlurSeparable(input);
    for (int y = 3; y < H - 3; y++) {
        for (int x = 3; x < W - 3; x++) {
            EXPECT_NEAR(output.data[y * W + x], 128, 1);
        }
    }
}

// Test 2: All black stays black
TEST(SeparableGaussianTest, AllBlackStaysBlack) {
    int W = 64, H = 64;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 0);
    Image output = gaussianBlurSeparable(input);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            EXPECT_EQ(output.data[y * W + x], 0);
        }
    }
}

// Test 3: Output dimensions match input
TEST(SeparableGaussianTest, OutputDimensionsMatchInput) {
    int W = 100, H = 75; // non-power-of-two
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 50);
    Image output = gaussianBlurSeparable(input);
    EXPECT_EQ(output.width,  W);
    EXPECT_EQ(output.height, H);
}

// Test 4: Impulse spreads symmetrically (same property as 2D version)
TEST(SeparableGaussianTest, ImpulseSpreadsSymmetrically) {
    int W = 32, H = 32;
    int cx = W / 2;
    int cy = H / 2;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 0);
    input.data[cy * W + cx] = 255;
    Image output = gaussianBlurSeparable(input);
    EXPECT_EQ(output.data[cy * W + (cx-1)], output.data[cy * W + (cx+1)]);
    EXPECT_EQ(output.data[(cy-1) * W + cx], output.data[(cy+1) * W + cx]);
    EXPECT_GT(output.data[cy * W + cx],     output.data[cy * W + (cx+1)]);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
