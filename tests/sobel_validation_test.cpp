#include <iostream>
#include <cstdlib>
#include "types.h"
#include "sobel.h"

void run_sobel_validation() {
    // --- Test 1: Diagonal gradient 512x512 ---
    std::cout << "Test 1: Diagonal gradient 512x512...\n";
    Image input;
    input.width = 512;
    input.height = 512;
    input.data.resize(512 * 512);
    for (int y = 0; y < 512; y++)
        for (int x = 0; x < 512; x++)
            input.data[y * 512 + x] = (uint8_t)((x + y) % 256);

    Image16 scalar_gx = sobelX(input);
    Image16 scalar_gy = sobelY(input);
    Image16 rvv_gx, rvv_gy;
    sobel_fused_rvv(input, rvv_gx, rvv_gy);

    for (int i = 0; i < 512 * 512; i++) {
        if (scalar_gx.data[i] != rvv_gx.data[i] || scalar_gy.data[i] != rvv_gy.data[i]) {
            std::cout << "FAIL at pixel " << i << "\n";
            std::cout << "  Scalar: Gx=" << scalar_gx.data[i] << " Gy=" << scalar_gy.data[i] << "\n";
            std::cout << "  RVV:    Gx=" << rvv_gx.data[i]    << " Gy=" << rvv_gy.data[i]    << "\n";
            exit(1);
        }
    }
    std::cout << "PASS\n";

    // --- Test 2: Non-power-of-two size (forces tail case) ---
    std::cout << "Test 2: Non-power-of-two 100x75...\n";
    Image input2;
    input2.width = 100;
    input2.height = 75;
    input2.data.resize(100 * 75);
    for (int i = 0; i < 100 * 75; i++)
        input2.data[i] = (uint8_t)(i % 256);

    Image16 s_gx2 = sobelX(input2);
    Image16 s_gy2 = sobelY(input2);
    Image16 r_gx2, r_gy2;
    sobel_fused_rvv(input2, r_gx2, r_gy2);

    for (int i = 0; i < 100 * 75; i++) {
        if (s_gx2.data[i] != r_gx2.data[i] || s_gy2.data[i] != r_gy2.data[i]) {
            std::cout << "FAIL at pixel " << i << "\n";
            exit(1);
        }
    }
    std::cout << "PASS\n";

    // --- Test 3: Uniform image (all gradients must be zero) ---
    std::cout << "Test 3: Uniform image (expect all zeros)...\n";
    Image input3;
    input3.width = 256;
    input3.height = 256;
    input3.data.assign(256 * 256, 128);

    Image16 r_gx3, r_gy3;
    sobel_fused_rvv(input3, r_gx3, r_gy3);

    for (int i = 0; i < 256 * 256; i++) {
        if (r_gx3.data[i] != 0 || r_gy3.data[i] != 0) {
            std::cout << "FAIL: non-zero gradient on uniform image at pixel " << i << "\n";
            exit(1);
        }
    }
    std::cout << "PASS\n";

    std::cout << "\nAll tests passed!\n";
}

int main() {
    run_sobel_validation();
    return 0;
}
