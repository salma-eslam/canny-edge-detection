#include <gtest/gtest.h>
#include "../src/types.h"
#include "../src/direction.h"

// Test 1: uniform flat region 
TEST(CannyDirectionTest, HandlesZeroGradients) {
    Image16 gx; gx.width = 1; gx.height = 1; gx.data = {0};
    Image16 gy; gy.width = 1; gy.height = 1; gy.data = {0};
    
    Image result = gradientDirection(gx, gy);
    EXPECT_EQ((int)result.data[0], 0);
}

// Test 2: strong horizontal push  (0)
TEST(CannyDirectionTest, ClassifiesHorizontalEdges) {
    Image16 gx; gx.width = 1; gx.height = 1; gx.data = {100};
    Image16 gy; gy.width = 1; gy.height = 1; gy.data = {10};
    
    Image result = gradientDirection(gx, gy);
    EXPECT_EQ((int)result.data[0], 0);
}

// Test 3: strong vertical push 
TEST(CannyDirectionTest, ClassifiesVerticalEdges) {
    Image16 gx; gx.width = 1; gx.height = 1; gx.data = {5};
    Image16 gy; gy.width = 1; gy.height = 1; gy.data = {200};
    
    Image result = gradientDirection(gx, gy);
    EXPECT_EQ((int)result.data[0], 2);
}

// Test 4: equal positive forces output (45)
TEST(CannyDirectionTest, Classifies45DegreeDiagonals) {
    Image16 gx; gx.width = 1; gx.height = 1; gx.data = {50};
    Image16 gy; gy.width = 1; gy.height = 1; gy.data = {50};
    
    Image result = gradientDirection(gx, gy);
    EXPECT_EQ((int)result.data[0], 1);
}

// Test 5: opposite signs output (135)
TEST(CannyDirectionTest, Classifies135DegreeDiagonals) {
    Image16 gx; gx.width = 1; gx.height = 1; gx.data = {-50};
    Image16 gy; gy.width = 1; gy.height = 1; gy.data = {50};
    
    Image result = gradientDirection(gx, gy);
    EXPECT_EQ((int)result.data[0], 3);
}
