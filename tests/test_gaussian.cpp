#include <gtest/gtest.h>
#include "image_io.h"
#include "gaussian.h"
// Test 1: Uniform image (all pixels = 128) should stay uniform after blur
TEST(GaussianBlurTest, UniformImageStaysUniform) {
    int W = 64, H = 64;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 128);
    Image output = gaussianBlur(input);
    for (int y =3; y < H -3; y++) {
        for (int x = 3; x < W - 3; x++) {
            EXPECT_NEAR(output.data[y * W + x], 128, 1);
        }
    }
}

// Test 2: All black image should stay all black after blur
TEST(GaussianBlurTest, AllBlackStaysBlack) {
    int W = 64, H = 64;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 0);
    Image output = gaussianBlur(input);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            EXPECT_EQ(output.data[y * W + x], 0);
        }
    }
}
// Test 3: Single bright pixel should spread symmetrically to neighbors
TEST(GaussianBlurTest, ImpulseSpreadsSymmetrically) {
    int W = 32, H = 32;
    int cx = W / 2;
    int cy = H / 2;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 0);
    input.data[cy * W + cx] = 255;
    Image output = gaussianBlur(input);
    EXPECT_EQ(output.data[cy * W + (cx-1)], output.data[cy * W + (cx+1)]);
    EXPECT_EQ(output.data[(cy-1) * W + cx], output.data[(cy+1) * W + cx]);
    EXPECT_GT(output.data[cy * W + cx],     output.data[cy * W + (cx+1)]);
}
// Test 4: Output image must have same dimensions as input
TEST(GaussianBlurTest, OutputDimensionsMatchInput) {
    int W = 100, H = 80;
    Image input;
    input.width  = W;
    input.height = H;
    input.data.resize(W * H, 50);
    Image output = gaussianBlur(input);
    EXPECT_EQ(output.width,  W);
    EXPECT_EQ(output.height, H);
    EXPECT_EQ((int)output.data.size(), W * H);
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
